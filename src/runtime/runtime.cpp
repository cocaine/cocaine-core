/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/common.hpp"
#include "cocaine/context.hpp"

#include "cocaine/detail/runtime/logging.hpp"

#include "cocaine/detail/trace/logger.hpp"

#if !defined(__APPLE__)
    #include "cocaine/detail/runtime/pid_file.hpp"
#endif

#include <csignal>
#include <iostream>

#include <asio/io_service.hpp>
#include <asio/signal_set.hpp>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <blackhole/attribute.hpp>
#include <blackhole/attributes.hpp>
#include <blackhole/builder.hpp>
#include <blackhole/config/json.hpp>
#include <blackhole/extensions/facade.hpp>
#include <blackhole/extensions/writer.hpp>
#include <blackhole/formatter/json.hpp>
#include <blackhole/formatter/string.hpp>
#include <blackhole/handler/blocking.hpp>
#include <blackhole/logger.hpp>
#include <blackhole/record.hpp>
#include <blackhole/registry.hpp>
#include <blackhole/root.hpp>
#include <blackhole/sink/console.hpp>
#include <blackhole/sink/file.hpp>
#include <blackhole/sink/socket/udp.hpp>
#include <blackhole/wrapper.hpp>

#include "cocaine/logging.hpp"

#if defined(__linux__)
    #define BACKWARD_HAS_BFD 1
#endif

#include "backward.hpp"

using namespace cocaine;

namespace fs = boost::filesystem;
namespace ph = std::placeholders;
namespace po = boost::program_options;

namespace {

void
stacktrace(int signum, siginfo_t* COCAINE_UNUSED_(info), void* context) {
    ucontext_t* uctx = static_cast<ucontext_t*>(context);

    backward::StackTrace trace;
    backward::Printer printer;

#if defined(REG_RIP)
    void* error_address = reinterpret_cast<void*>(uctx->uc_mcontext.gregs[REG_RIP]);
#elif defined(REG_EIP)
    void* error_address = reinterpret_cast<void*>(uctx->uc_mcontext.gregs[REG_EIP]);
#else
    void* error_address = uctx = nullptr;
#endif

    if(error_address) {
        trace.load_from(error_address, 32);
    } else {
        trace.load_here(32);
    }

    printer.address = true;
    printer.print(trace);

    // Re-raise so that a core dump is generated.
    std::raise(signum);

    // Just in case, if the default handler returns for some weird reason.
    std::_Exit(EXIT_FAILURE);
}

struct runtime_t {
    runtime_t() {
        // Establish an alternative signal stack
        const size_t alt_stack_size = 8 * 1024 * 1024;

        m_alt_stack.ss_sp = new char[alt_stack_size];
        m_alt_stack.ss_size = alt_stack_size;
        m_alt_stack.ss_flags = 0;

        if(::sigaltstack(&m_alt_stack, nullptr) != 0) {
            std::cerr << "ERROR: Unable to activate an alternative signal stack" << std::endl;
        }

        // Reroute the core-generating signals.

        struct sigaction action;

        std::memset(&action, 0, sizeof(action));

        action.sa_sigaction = &stacktrace;
        action.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND | SA_SIGINFO;

        ::sigaction(SIGABRT, &action, nullptr);
        ::sigaction(SIGBUS,  &action, nullptr);
        ::sigaction(SIGSEGV, &action, nullptr);

        // Block the deprecated signals.

        sigset_t sigset;

        sigemptyset(&sigset);
        sigaddset(&sigset, SIGPIPE);

        ::sigprocmask(SIG_BLOCK, &sigset, nullptr);
    }

   ~runtime_t() {
        m_alt_stack.ss_flags = SS_DISABLE;

        if(::sigaltstack(&m_alt_stack, nullptr) != 0) {
            std::cerr << "ERROR: Unable to deactivate an alternative signal stack" << std::endl;
        }

        delete[] static_cast<char*>(m_alt_stack.ss_sp);
    }

    int
    run() {
        sigset_t sigset;

        sigemptyset(&sigset);
        sigaddset(&sigset, SIGINT);
        sigaddset(&sigset, SIGTERM);
        sigaddset(&sigset, SIGQUIT);

        int signum = -1;

        ::sigwait(&sigset, &signum);

        static const std::map<int, std::string> descriptions = {
            { SIGINT,  "SIGINT"  },
            { SIGQUIT, "SIGQUIT" },
            { SIGTERM, "SIGTERM" }
        };

        std::cout << "[Runtime] Caught " << descriptions.at(signum) << ", exiting." << std::endl;

        // There's no way it can actually go wrong.
        return EXIT_SUCCESS;
    }

private:
    // An alternative signal stack for SIGSEGV handling.
    stack_t m_alt_stack;
};

} // namespace

int
main(int argc, char* argv[]) {
    po::options_description general_options("General options");
    po::variables_map vm;

    general_options.add_options()
        ("help,h", "show this message")
        ("configuration,c", po::value<std::string>(), "location of the configuration file")
        ("logging,l", po::value<std::string>()->default_value("core"), "logging backend")
#if !defined(__APPLE__)
        ("daemonize,d", "daemonize on start")
        ("pidfile,p", po::value<std::string>(), "location of a pid file")
#endif
        ("version,v", "show version and build information");

    try {
        po::store(po::command_line_parser(argc, argv).options(general_options).run(), vm);
        po::notify(vm);
    } catch(const po::error& e) {
        std::cerr << cocaine::format("ERROR: %s.", e.what()) << std::endl;
        return EXIT_FAILURE;
    }

    if(vm.count("help")) {
        std::cout << cocaine::format("USAGE: %s [options]", argv[0]) << std::endl;
        std::cout << general_options;
        return EXIT_SUCCESS;
    }

    if(vm.count("version")) {
        std::cout << cocaine::format("Cocaine %d.%d.%d", COCAINE_VERSION_MAJOR, COCAINE_VERSION_MINOR,
            COCAINE_VERSION_RELEASE) << std::endl;
        return EXIT_SUCCESS;
    }

    // Validation

    if(!vm.count("configuration")) {
        std::cerr << "ERROR: no configuration file location has been specified." << std::endl;
        return EXIT_FAILURE;
    }

    // Startup

    std::unique_ptr<config_t> config;

    std::cout << "[Runtime] Parsing the configuration." << std::endl;

    try {
        config.reset(new config_t(vm["configuration"].as<std::string>()));
    } catch(const std::system_error& e) {
        std::cerr << cocaine::format("ERROR: unable to initialize the configuration - %s.", e.what()) << std::endl;
        return EXIT_FAILURE;
    }

#if !defined(__APPLE__)
    std::unique_ptr<pid_file_t> pidfile;

    if(vm.count("daemonize")) {
        if(daemon(0, 0) < 0) {
            std::cerr << "ERROR: daemonization failed." << std::endl;
            return EXIT_FAILURE;
        }

        fs::path pid_path;

        if(!vm["pidfile"].empty()) {
            pid_path = vm["pidfile"].as<std::string>();
        } else {
            pid_path = cocaine::format("%s/cocained.pid", config->path.runtime);
        }

        try {
            pidfile.reset(new pid_file_t(pid_path));
        } catch(const std::system_error& e) {
            std::cerr << cocaine::format("ERROR: unable to create the pidfile - %s.", e.what()) << std::endl;
            return EXIT_FAILURE;
        }
    }
#endif

    // Logging

    const auto backend = vm["logging"].as<std::string>();

    std::cout << cocaine::format("[Runtime] Initializing the logging system, backend: %s.", backend)
              << std::endl;

    std::unique_ptr<logging::logger_t> root;
    std::unique_ptr<logging::logger_t> logger;

    auto registry = blackhole::registry_t::configured();
    registry.add<blackhole::formatter::json_t>();
    registry.add<logging::console_t>();
    registry.add<blackhole::sink::file_t>();
    registry.add<blackhole::sink::socket::udp_t>();

    try {
        std::stringstream stream;
        stream << boost::lexical_cast<std::string>(config->logging.loggers);

        auto log = registry.builder<blackhole::config::json_t>(stream)
            .build("core");

        log.filter([&](const blackhole::record_t& record) -> bool {
            return record.severity() >= config->logging.severity;
        });

        root.reset(new blackhole::root_logger_t(std::move(log)));
        logger.reset(new logging::trace_wrapper_t(*root));
    } catch(const std::exception& e) {
        std::cerr << "ERROR: unable to initialize the logging: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    COCAINE_LOG_INFO(logger, "initializing the server");

    std::unique_ptr<context_t> context;

    try {
        context.reset(new context_t(*config, std::move(logger)));
    } catch(const std::system_error& e) {
        std::cout << cocaine::format("[Runtime] unable to initialize the context - %s.", e.what())
                  << std::endl;
        return EXIT_FAILURE;
    }

    return runtime_t().run();
}
