/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/asio/reactor.hpp"

#include "cocaine/context.hpp"

#ifndef __APPLE__
    #include "cocaine/detail/runtime/pid_file.hpp"
#endif

#include <csignal>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#ifdef __linux__
    #define BACKWARD_HAS_BFD 1
#endif

#include "backward.hpp"

using namespace cocaine;

namespace fs = boost::filesystem;
namespace po = boost::program_options;

namespace {
    void stacktrace(int signum, siginfo_t* info, void* context) {
        using namespace backward;

        (void)context;

        StackTrace trace;
        Printer printer;

        if(signum == SIGSEGV || signum == SIGBUS) {
            trace.load_from(info->si_addr, 32);
        } else {
            trace.load_here(32);
        }

        printer.print(trace);

        // Re-raise so that a core dump is generated.
        std::raise(signum);

        // Just in case, if the default handler returns for some weird reason.
        std::exit(EXIT_FAILURE);
    }

    struct runtime_t {
        runtime_t()
#if defined(EVFLAG_SIGNALFD)
            : m_loop(EVFLAG_SIGNALFD)
#endif
        {
            m_sigint.set<runtime_t, &runtime_t::terminate>(this);
            m_sigint.start(SIGINT);

            m_sigterm.set<runtime_t, &runtime_t::terminate>(this);
            m_sigterm.start(SIGTERM);

            m_sigquit.set<runtime_t, &runtime_t::terminate>(this);
            m_sigquit.start(SIGQUIT);

            // Establish an alternative signal stack

            size_t alt_stack_size = 8 * 1024 * 1024;

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

            sigset_t signals;

            sigemptyset(&signals);
            sigaddset(&signals, SIGPIPE);

            ::sigprocmask(SIG_BLOCK, &signals, nullptr);
        }

       ~runtime_t() {
            m_alt_stack.ss_flags = SS_DISABLE;

            if(::sigaltstack(&m_alt_stack, nullptr) != 0) {
                std::cerr << "ERROR: Unable to deactivate an alternative signal stack" << std::endl;
            }

            auto ptr = static_cast<char*>(m_alt_stack.ss_sp);

            delete[] ptr;
        }

        void
        run() {
            m_loop.loop();
        }

    private:
        void
        terminate(ev::sig&, int) {
            m_loop.unloop(ev::ALL);
        }

    private:
        // Main event loop, able to handle signals.
        ev::default_loop m_loop;

        // Signal watchers.
        ev::sig m_sigint;
        ev::sig m_sigterm;
        ev::sig m_sigquit;

        // An alternative signal stack for SIGSEGV handling.
        stack_t m_alt_stack;
    };
}

int
main(int argc, char* argv[]) {
    po::options_description general_options("General options");
    po::variables_map vm;

    general_options.add_options()
        ("help,h", "show this message")
        ("configuration,c", po::value<std::string>(), "location of the configuration file")
#ifndef __APPLE__
        ("daemonize,d", "daemonize on start")
        ("pidfile,p", po::value<std::string>(), "location of a pid file")
#endif
        ("version,v", "show version and build information");

    try {
        po::store(
            po::command_line_parser(argc, argv).
                options(general_options).
                run(),
            vm);
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
        std::cout << cocaine::format(
            "Cocaine %d.%d.%d",
            COCAINE_VERSION_MAJOR,
            COCAINE_VERSION_MINOR,
            COCAINE_VERSION_RELEASE
        ) << std::endl;

        return EXIT_SUCCESS;
    }

    // Validation

    if(!vm.count("configuration")) {
        std::cerr << "ERROR: no configuration file location has been specified." << std::endl;
        return EXIT_FAILURE;
    }

    // Startup

    std::unique_ptr<config_t> config;

    try {
        config.reset(new config_t(vm["configuration"].as<std::string>()));
    } catch(const cocaine::error_t& e) {
        std::cerr << cocaine::format(
            "ERROR: unable to initialize the configuration - %s.",
            e.what()
        ) << std::endl;

        return EXIT_FAILURE;
    }

#ifndef __APPLE__
    std::unique_ptr<pid_file_t> pidfile;

    if(vm.count("daemonize")) {
        fs::path pid_path;

        if(!vm["pidfile"].empty()) {
            pid_path = fs::path(vm["pidfile"].as<std::string>());
        } else {
            pid_path = cocaine::format("%s/cocained.pid", config->path.runtime);
        }

        if(daemon(0, 0) < 0) {
            std::cerr << "ERROR: daemonization failed." << std::endl;
            return EXIT_FAILURE;
        }

        try {
            pidfile.reset(new pid_file_t(pid_path));
        } catch(const cocaine::error_t& e) {
            std::cerr << cocaine::format(
                "ERROR: unable to create the pidfile - %s.",
                e.what()
            ) << std::endl;

            return EXIT_FAILURE;
        }
    }
#endif

    // NOTE: The default event loop have to initialized first, otherwise
    // signals wouldn't be properly handled. Probably, a bug.
    runtime_t runtime;

    std::unique_ptr<context_t> context;

    try {
        context.reset(new context_t(*config, "core"));
    } catch(const std::exception& e) {
        std::cerr << cocaine::format(
            "ERROR: unable to initialize the context - %s.",
            e.what()
        ) << std::endl;

        return EXIT_FAILURE;
    }

    runtime.run();

    return EXIT_SUCCESS;
}
