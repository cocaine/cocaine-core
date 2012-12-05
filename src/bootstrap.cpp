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

#include "cocaine/config.hpp"
#include "cocaine/context.hpp"

#include "cocaine/api/service.hpp"

#include "cocaine/helpers/pid_file.hpp"
#include "cocaine/helpers/format.hpp"

#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

using namespace cocaine;

namespace po = boost::program_options;
namespace fs = boost::filesystem;

int main(int argc, char * argv[]) {
    po::options_description general_options("General options"),
                            service_options("Service options"),
                            combined_options;
    
    po::variables_map vm;

    general_options.add_options()
        ("help,h", "show this message")
        ("version,v", "show version and build information")
        ("configuration,c", po::value<std::string>(), "location of the configuration file")
        ("daemonize,d", "daemonize on start")
        ("pidfile,p", po::value<std::string>(), "location of a pid file");

    service_options.add_options()
        ("service,s", po::value<std::string>(), "name of a service to launch");
    
    combined_options.add(general_options)
                    .add(service_options);

    try {
        po::store(
            po::command_line_parser(argc, argv).
                options(combined_options).
                run(),
            vm);
        po::notify(vm);
    } catch(const po::unknown_option& e) {
        std::cerr << cocaine::format("Error: %s.", e.what()) << std::endl;
        return EXIT_FAILURE;
    } catch(const po::ambiguous_option& e) {
        std::cerr << cocaine::format("Error: %s.", e.what()) << std::endl;
        return EXIT_FAILURE;
    }

    if(vm.count("help")) {
        std::cout << cocaine::format("Usage: %s <service-name> [options]", argv[0]) << std::endl;
        std::cout << combined_options;
        return EXIT_SUCCESS;
    }

    if(vm.count("version")) {
        std::cout << cocaine::format("Cocaine %d", COCAINE_VERSION) << std::endl;
        return EXIT_SUCCESS;
    }

    // Validation

    if(!vm.count("configuration")) {
        std::cerr << "Error: no configuration file location has been specified." << std::endl;
        return EXIT_FAILURE;
    }

    if(!vm.count("service")) {
        std::cerr << "Error: no service name has been specified." << std::endl;
        return EXIT_FAILURE;
    }
    
    // Startup

    std::string cfg = vm["configuration"].as<std::string>();
    std::string service = vm["service"].as<std::string>();

    std::unique_ptr<config_t> config;

    try {
        config.reset(new config_t(cfg));
    } catch(const std::exception& e) {
        std::cerr << cocaine::format("Error: unable to initialize the configuration - %s.", e.what()) << std::endl;
        return EXIT_FAILURE;
    }

    std::unique_ptr<pid_file_t> pidfile;

    if(vm.count("daemonize")) {
        fs::path pid_path;

        if(!vm["pidfile"].empty()) {
            pid_path = fs::path(vm["pidfile"].as<std::string>());
        } else {
            pid_path = fs::path(config->path.runtime) / cocaine::format("%s.pid", service);
        }

        if(daemon(0, 0) < 0) {
            std::cerr << "Error: daemonization failed." << std::endl;
            return EXIT_FAILURE;
        }

        try {
            pidfile.reset(new pid_file_t(pid_path));
        } catch(const std::exception& e) {
            std::cerr << cocaine::format("Error: %s.", e.what()) << std::endl;
            return EXIT_FAILURE;
        }
    }

    std::unique_ptr<context_t> context;

    try {
        context.reset(new context_t(*config, service));
    } catch(const std::exception& e) {
        std::cerr << cocaine::format("Error: unable to initialize the context - %s.", e.what()) << std::endl;
        return EXIT_FAILURE;
    }

    api::service_ptr_t runnable;

    try {
        runnable = api::service(*context, service);
    } catch(const std::exception& e) {
        std::cerr << cocaine::format("Error: unable to start the service - %s.", e.what()) << std::endl;
        return EXIT_FAILURE;
    }

    runnable->run();

    return EXIT_SUCCESS;
}
