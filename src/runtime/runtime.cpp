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
#include "cocaine/context/config.hpp"
#include "cocaine/context/signal.hpp"
#include "cocaine/dynamic.hpp"
#include "cocaine/errors.hpp"
#include "cocaine/idl/context.hpp"
#include "cocaine/signal.hpp"

#include "cocaine/detail/runtime/logging.hpp"

#include "cocaine/detail/trace/logger.hpp"

#if !defined(__APPLE__)
    #include "cocaine/detail/runtime/pid_file.hpp"
#endif

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
#include <blackhole/sink/socket/tcp.hpp>
#include <blackhole/sink/socket/udp.hpp>
#include <blackhole/wrapper.hpp>

#include "cocaine/logging.hpp"

#if defined(__linux__)
    #define BACKWARD_HAS_BFD 1
#endif

#include <backward.hpp>

#include <condition_variable>
#include <csignal>
#include <iostream>
#include <thread>

using namespace cocaine;

namespace fs = boost::filesystem;
namespace ph = std::placeholders;
namespace po = boost::program_options;

namespace {

std::mutex finalizer_mutex;
std::condition_variable finalizer_cv;
bool finalize = false;

struct sighup_handler_t {
    blackhole::root_logger_t& logger;
    logging::logger_t& wrapper;
    blackhole::registry_t& registry;
    cocaine::signal::handler_base_t& sig_handler;
    cocaine::context_t& context;

    void
    operator()(const std::error_code& ec, int signum, const siginfo_t& info) {
        if(ec == std::errc::operation_canceled) {
            return;
        }

        // We do not suspect any other error codes except oeration cancellation.
        BOOST_ASSERT(!ec);

        COCAINE_LOG_INFO(wrapper, "resetting logger");
        std::stringstream stream;
        stream << boost::lexical_cast<std::string>(context.config().logging().loggers());

        // Create a new logger and set the filter before swap to be sure that no events are missed.
        auto log = registry.builder<blackhole::config::json_t>(stream)
            .build("core");

        const auto severity = context.config().logging().severity();
        log.filter([=](const blackhole::record_t& record) -> bool {
            return record.severity() >= severity || !trace_t::current().empty();
        });

        logger = std::move(log);

        context.signal_hub().invoke<cocaine::io::context::os_signal>(signum, info);
        sig_handler.async_wait(SIGHUP, *this);
    }
};

struct sigchild_handler_t {
    cocaine::context_t& context;
    cocaine::signal::handler_base_t& handler;
    void
    operator()(const std::error_code& ec, int signum, const siginfo_t& info) {
        if(ec == std::errc::operation_canceled) {
            return;
        }
        assert(!ec);
        context.signal_hub().invoke<cocaine::io::context::os_signal>(signum, info);
        handler.async_wait(signum, *this);
    }
};

void terminate() {
    {
        std::lock_guard<std::mutex> lock(finalizer_mutex);
        finalize = true;
    }
    finalizer_cv.notify_one();
}

struct terminate_handler_t {
    void
    operator()(const std::error_code& ec, int) {
        if(ec != std::errc::operation_canceled) {
            assert(!ec);
            terminate();
        }
    }
};

struct sigpipe_handler_t {
    cocaine::signal::handler_base_t& handler;

    void
    operator()(const std::error_code& ec, int signum) {
        if(ec != std::errc::operation_canceled) {
            handler.async_wait(signum, *this);
        }
    }
};

void
run_signal_handler(cocaine::signal::handler_t& signal_handler, logging::logger_t& logger){
    try {
        signal_handler.run();
    } catch (const std::system_error& e) {
        COCAINE_LOG_ERROR(logger, "exception in signal handler - {}", error::to_string(e));
    }
}

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
        std::cerr << cocaine::format("ERROR: {}.", e.what()) << std::endl;
        return EXIT_FAILURE;
    }

    if(vm.count("help")) {
        std::cout << cocaine::format("USAGE: {} [options]", argv[0]) << std::endl;
        std::cout << general_options;
        return EXIT_SUCCESS;
    }

    if(vm.count("version")) {
        std::cout << cocaine::format("Cocaine {}.{}.{}", COCAINE_VERSION_MAJOR, COCAINE_VERSION_MINOR,
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
        config = make_config(vm["configuration"].as<std::string>());
    } catch(const std::system_error& e) {
        std::cerr << cocaine::format("ERROR: unable to initialize the configuration - {}.", error::to_string(e)) << std::endl;
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
            pid_path = cocaine::format("{}/cocained.pid", config->path.runtime);
        }

        try {
            pidfile.reset(new pid_file_t(pid_path));
        } catch(const std::system_error& e) {
            std::cerr << cocaine::format("ERROR: unable to create the pidfile - {}.", error::to_string(e)) << std::endl;
            return EXIT_FAILURE;
        }
    }
#endif

    // Logging

    const auto backend = vm["logging"].as<std::string>();

    std::cout << cocaine::format("[Runtime] Initializing the logging system, backend: {}.", backend)
              << std::endl;

    std::unique_ptr<blackhole::root_logger_t> root;
    std::unique_ptr<logging::logger_t> logger;

    auto registry = blackhole::registry_t::configured();
    registry.add<blackhole::formatter::json_t>();
    registry.add<logging::console_t>();
    registry.add<blackhole::sink::file_t>();
    registry.add<blackhole::sink::socket::tcp_t>();
    registry.add<blackhole::sink::socket::udp_t>();

    try {
        std::stringstream stream;
        stream << boost::lexical_cast<std::string>(config->logging().loggers());

        auto log = registry.builder<blackhole::config::json_t>(stream)
            .build("core");

        auto severity = config->logging().severity();
        log.filter([=](const blackhole::record_t& record) -> bool {
            return record.severity() >= severity || !trace_t::current().empty();
        });

        root.reset(new blackhole::root_logger_t(std::move(log)));
        logger.reset(new logging::trace_wrapper_t(*root));
    } catch(const std::exception& e) {
        std::cerr << "ERROR: unable to initialize the logging: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    COCAINE_LOG_INFO(logger, "initializing the server");
    std::unique_ptr<cocaine::logging::logger_t> wrapper(new blackhole::wrapper_t(*root, {{"source", "signal_handler"}}));
    auto wrapper_ref = std::ref(*wrapper);
    std::set<int> signals = { SIGPIPE, SIGINT, SIGQUIT, SIGTERM, SIGCHLD, SIGHUP };
    signal::handler_t signal_handler(std::move(wrapper), signals);

    // Set handlers for signals
    signal_handler.async_wait(SIGPIPE, sigpipe_handler_t{signal_handler});
    signal_handler.async_wait(SIGINT, terminate_handler_t());
    signal_handler.async_wait(SIGQUIT, terminate_handler_t());
    signal_handler.async_wait(SIGTERM, terminate_handler_t());

    // Start signal handling thread
    std::thread sig_thread(&run_signal_handler, std::ref(signal_handler), wrapper_ref);

    // Run context
    std::unique_ptr<context_t> context;
    try {
        context = get_context(std::move(config), std::move(logger));
    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(root, "unable to initialize the context - {}.", error::to_string(e));
        signal_handler.stop();
        sig_thread.join();
        return 1;
    }


    // Handlers for context os_signal slot
    auto hup_handler_cancellation = signal_handler.async_wait(SIGHUP, sighup_handler_t{*root, wrapper_ref.get(), registry, signal_handler, *context});
    auto child_handler_cancellation = signal_handler.async_wait(SIGCHLD, sigchild_handler_t{*context, signal_handler});

    // Wait until signaling termination
    std::unique_lock<std::mutex> lock(finalizer_mutex);
    finalizer_cv.wait(lock, [&] { return finalize; });

    // unlock the mutex, as we don't need it anymore to prevent deadlock with several terminate calls
    lock.unlock();

    // Termination
    if(context) {
        context->terminate();
    }
    hup_handler_cancellation.cancel();
    child_handler_cancellation.cancel();
    context.reset(nullptr);
    signal_handler.stop();
    sig_thread.join();
    return 0;
}
