/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include <boost/program_options.hpp>
#include <iostream>

#include "cocaine/config.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/server/pid_file.hpp"
#include "cocaine/server/server.hpp"

using namespace cocaine;

namespace po = boost::program_options;

int main(int argc, char * argv[]) {
    po::options_description general_options("General options"),
                            server_options("Server options"),
                            combined_options;
    
    po::positional_options_description positional_options;
    
    po::variables_map vm;

    general_options.add_options()
        ("help,h", "show this message")
        ("version,v", "show version and build information")
        ("configuration,c", po::value<std::string>
            ()->default_value("/etc/cocaine/cocaine.conf"),
            "location of the configuration file")
        ("daemonize,d", "daemonize on start")
        ("pidfile,p", po::value<std::string>
            ()->default_value("/var/run/cocaine/cocained.pid"),
            "location of a pid file");

    server_config_t server_config;

    server_options.add_options()
        ("server:runlist,r", po::value<std::string>
            (&server_config.runlist)->default_value("default"),
            "server runlist name")
        ("server:listen", po::value< std::vector<std::string> >
            (&server_config.listen_endpoints)->composing(),
            "server listen endpoints, can be specified multiple times")
        ("server:announce", po::value< std::vector<std::string> >
            (&server_config.announce_endpoints)->composing(),
            "server announce endpoints, can be specified multiple times")
        ("server:announce-interval", po::value<float>
            (&server_config.announce_interval)->default_value(5.0f),
            "server announce interval");

    positional_options.add("server:listen", -1);

    combined_options.add(general_options)
                    .add(server_options);

    try {
        po::store(
            po::command_line_parser(argc, argv).
                options(combined_options).
                positional(positional_options).
                run(),
            vm);
        po::notify(vm);
    } catch(const po::unknown_option& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch(const po::ambiguous_option& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    if(vm.count("help")) {
        std::cout << "Usage: " << argv[0] << " endpoint-list [options]" << std::endl;
        std::cout << general_options << server_options;
        return EXIT_SUCCESS;
    }

    if(vm.count("version")) {
        std::cout << "Cocaine " << COCAINE_VERSION << std::endl;
        return EXIT_SUCCESS;
    }

    // Validation

    if(!vm.count("configuration")) {
        std::cerr << "Error: no configuration file location has been specified." << std::endl;
        return EXIT_FAILURE;
    }

    // Startup

    std::unique_ptr<pid_file_t> pidfile;

    if(vm.count("daemonize")) {
        if(daemon(0, 0) < 0) {
            std::cerr << "Error: daemonization failed." << std::endl;
            return EXIT_FAILURE;
        }

        try {
            pidfile.reset(new pid_file_t(vm["pidfile"].as<std::string>()));
        } catch(const cocaine::error_t& e) {
            std::cerr << "Error: " << e.what() << "." << std::endl;
            return EXIT_FAILURE;
        }
    }

    std::unique_ptr<context_t> context;

    try {
        context.reset(new context_t(vm["configuration"].as<std::string>()));
    } catch(const std::exception& e) {
        std::cerr << "Error: unable to initialize the context - " << e.what();
        return EXIT_FAILURE;
    }

    boost::shared_ptr<logging::logger_t> log(context->log("main"));

    COCAINE_LOG_INFO(log, "starting the server");

    std::unique_ptr<server_t> server;
    
    try {
        server.reset(
            new server_t(
                *context,
                server_config
            )
        );
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(log, "unable to start the server - %s", e.what());
        return EXIT_FAILURE;
    }

    server->run();

    COCAINE_LOG_INFO(log, "the server has terminated");

    return EXIT_SUCCESS;
}
