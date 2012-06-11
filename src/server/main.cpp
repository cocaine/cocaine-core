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

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#include "cocaine/config.hpp"
#include "cocaine/context.hpp"

#include "cocaine/server/server.hpp"

#include "cocaine/loggers/syslog.hpp"

#include "cocaine/helpers/pid_file.hpp"

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
            ()->default_value("/etc/cocaine/default.json"),
            "location of the configuration file")
        ("daemonize,d", "daemonize on start")
        ("pidfile,p", po::value<std::string>
            ()->default_value("/var/run/cocaine/default.pid"),
            "location of a pid file")
        ("verbose", "produce a lot of output");

    server_config_t server_config;

    server_options.add_options()
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
    // ----------

    if(!vm.count("configuration")) {
        std::cerr << "Error: no configuration file location has been specified." << std::endl;
        return EXIT_FAILURE;
    }

    // Startup
    // -------

    context_t context(
        vm["configuration"].as<std::string>(),
        boost::make_shared<logging::syslog_t>(
            vm.count("verbose") ? logging::debug : logging::info,
            "cocaine"
        )
    );

    boost::shared_ptr<logging::logger_t> log(context.log("main"));

    /*
    if(vm.count("core:port-range")) {
        std::vector<std::string> limits;

        boost::algorithm::split(
            limits,
            vm["core:port-range"].as<std::string>(),
            boost::algorithm::is_any_of(":-")
        );

        if(limits.size() != 2) {
            std::cout << "Error: invalid port range format" << std::endl;
            return EXIT_FAILURE;
        }

        try {
            config.runtime.ports.assign(
                boost::make_counting_iterator(boost::lexical_cast<uint16_t>(limits[0])),
                boost::make_counting_iterator(boost::lexical_cast<uint16_t>(limits[1]))
            );
        } catch(const boost::bad_lexical_cast& e) {
            std::cout << "Error: invalid port range values" << std::endl;
            return EXIT_FAILURE;
        }
    }
    */

    std::auto_ptr<helpers::pid_file_t> pidfile;
    std::auto_ptr<server_t> server;

    log->info("starting the server");

    if(vm.count("daemonize")) {
        if(daemon(0, 0) < 0) {
            log->error("daemonization failed");
            return EXIT_FAILURE;
        }

        try {
            pidfile.reset(
                new helpers::pid_file_t(vm["pidfile"].as<std::string>())
            );
        } catch(const std::runtime_error& e) {
            log->error("%s", e.what());
            return EXIT_FAILURE;
        }
    }

    try {
        server.reset(
            new server_t(
                context,
                server_config
            )
        );
    } catch(const std::exception& e) {
        log->error("unable to start the server - %s", e.what());
        return EXIT_FAILURE;
    }

    server->run();

    log->info("the server has terminated");

    return EXIT_SUCCESS;
}
