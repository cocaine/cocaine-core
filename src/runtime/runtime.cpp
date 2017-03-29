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

#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <blackhole/attribute.hpp>
#include <blackhole/attributes.hpp>
#include <blackhole/config/json.hpp>
#include <blackhole/extensions/facade.hpp>
#include <blackhole/extensions/writer.hpp>
#include <blackhole/logger.hpp>
#include <blackhole/registry.hpp>
#include <blackhole/root.hpp>
#include <blackhole/wrapper.hpp>

#include "cocaine/common.hpp"
#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/context/signal.hpp"
#include "cocaine/dynamic.hpp"
#include "cocaine/errors.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/signal.hpp"

#include "cocaine/detail/runtime/logging.hpp"

#if !defined(__APPLE__)
    #include "cocaine/detail/runtime/pid_file.hpp"
#endif

#include "signal.hpp"

#if defined(__linux__)
    #define BACKWARD_HAS_BFD 1
#endif
#include <backward.hpp>

using namespace cocaine;

namespace po = boost::program_options;

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

        boost::filesystem::path pid_path;

        if(!vm["pidfile"].empty()) {
            pid_path = vm["pidfile"].as<std::string>();
        } else {
            pid_path = cocaine::format("{}/cocained.pid", config->path().runtime());
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

    auto registry = blackhole::registry::configured();
    registry->add<logging::console_t>();

    try {
        std::stringstream stream;
        stream << boost::lexical_cast<std::string>(config->logging().loggers());

        auto log = registry->builder<blackhole::config::json_t>(stream)
            .build(backend);

        root.reset(new blackhole::root_logger_t(std::move(log)));
        logger.reset(new blackhole::wrapper_t(*root, {}));
    } catch(const std::exception& e) {
        std::cerr << "ERROR: unable to initialize the logging: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    COCAINE_LOG_INFO(logger, "initializing the server");

    auto slog = std::make_shared<blackhole::wrapper_t>(*root, blackhole::attributes_t{
        {"source", "runtime/signals"}
    });

    // Signal handling.

    signal::handler_t handler(slog, {SIGPIPE, SIGINT, SIGQUIT, SIGTERM, SIGCHLD, SIGHUP});

    // Run context.
    std::unique_ptr<context_t> context;
    try {
        context = make_context(std::move(config), std::move(logger));
    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(root, "unable to initialize the context - {}.", error::to_string(e));
        return EXIT_FAILURE;
    }

    signal::engine_t engine(handler, slog);
    engine.on(SIGHUP, propagate_t{context->signal_hub(), handler, [&] {
        COCAINE_LOG_DEBUG(slog, "resetting logger");
        std::stringstream stream;
        stream << boost::lexical_cast<std::string>(context->config().logging().loggers());

        // Create a new logger before swap to be sure that no events are missed.
        *root = registry->builder<blackhole::config::json_t>(stream)
            .build(backend);
        COCAINE_LOG_INFO(slog, "core logger has been successfully reset");
    }});
    engine.on(SIGCHLD, propagate_t{context->signal_hub(), handler, {}});
    engine.ignore(SIGPIPE);
    engine.start();
    engine.wait_for({SIGINT, SIGTERM, SIGQUIT});

    return EXIT_SUCCESS;
}
