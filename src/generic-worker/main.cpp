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

#include "cocaine/generic-worker/worker.hpp"

using namespace cocaine;
using namespace cocaine::engine;

namespace po = boost::program_options;

int main(int argc, char * argv[]) {
    po::options_description general_options("General options"),
                            slave_options,
                            combined_options;
    
    po::variables_map vm;

    general_options.add_options()
        ("help,h", "show this message")
        ("version,v", "show version and build information")
        ("configuration,c", po::value<std::string>
            ()->default_value("/etc/cocaine/cocaine.conf"),
            "location of the configuration file");

    worker_config_t worker_config;

    slave_options.add_options()
        ("slave:app", po::value<std::string>
            (&worker_config.name))
        ("slave:profile", po::value<std::string>
            (&worker_config.profile))
        ("slave:uuid", po::value<std::string>
            (&worker_config.uuid));

    combined_options.add(general_options)
                    .add(slave_options);

    try {
        po::store(
            po::command_line_parser(argc, argv).
                options(combined_options).
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
        std::cout << general_options << slave_options;
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

    std::unique_ptr<context_t> context;

    try {
        context.reset(new context_t(vm["configuration"].as<std::string>()));
    } catch(const std::exception& e) {
        std::cerr << "Error: unable to initialize the context - " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::unique_ptr<worker_t> worker;

    try {
        worker.reset(
            new worker_t(
                *context,
                worker_config
            )
        );
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(
            context->log("main"),
            "unable to start the worker - %s",
            e.what()
        );
        
        return EXIT_FAILURE;
    }

    worker->run();

    return EXIT_SUCCESS;
}
