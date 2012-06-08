//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#include "cocaine/config.hpp"

#include "cocaine/slave/slave.hpp"

#include "cocaine/loggers/syslog.hpp"

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
            ()->default_value("/etc/cocaine/default.json"),
            "location of the configuration file")
        ("verbose", "produce a lot of output");

    slave_config_t slave_config;

    slave_options.add_options()
        ("slave:app", po::value<std::string>
            (&slave_config.app))
        ("slave:uuid", po::value<std::string>
            (&slave_config.uuid));

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

    std::auto_ptr<engine::slave_t> slave;

    try {
        slave.reset(
            new engine::slave_t(
                context,
                slave_config
            )
        );
    } catch(const std::exception& e) {
        log->error("unable to start the slave - %s", e.what());
        return EXIT_FAILURE;
    }

    slave->run();

    return EXIT_SUCCESS;
}
