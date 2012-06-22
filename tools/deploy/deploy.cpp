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

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/program_options.hpp>

#include "cocaine/config.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/package.hpp"

#include "cocaine/helpers/blob.hpp"
#include "cocaine/helpers/json.hpp"

using namespace cocaine;
using namespace cocaine::storages;

namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace {
    class stdio_sink_t:
        public logging::sink_t
    {
        public:
            stdio_sink_t(logging::priorities verbosity):
                cocaine::logging::sink_t(verbosity)
            { }

            virtual void emit(logging::priorities,
                              const std::string& source,
                              const std::string& message) const 
            {
                std::cout << source << ": " 
                          << message << std::endl;
            }
    };

    static void deploy(context_t& context,
                       const std::string& name,
                       const fs::path& manifest_path,
                       const fs::path& package_path)
    {
        std::string type,
                    compression;
        
        Json::Reader reader;
        Json::Value manifest;

        fs::ifstream manifest_stream(manifest_path);

        if(!manifest_stream) {
            std::cerr << "Error: unable to open '" << manifest_path << "'." << std::endl;
            return;
        }
        
        if(!reader.parse(manifest_stream, manifest)) {
            std::cerr << "Error: the app manifest in '" << manifest_path << "' is corrupted." << std::endl;
            std::cerr << reader.getFormattedErrorMessages() << std::endl;
            return;
        }

        type = manifest.get("type", "").asString();

        if(type.empty()) {
            std::cerr << "Error: no app type has been specified in the manifest." << std::endl;
            return;
        }

        fs::ifstream package_stream(
            package_path,
            std::fstream::binary | std::fstream::in
        );

        if(!package_stream) {
            std::cerr << "Error: unable to open '" << package_path << "'." << std::endl;
            return;
        }

        std::stringstream buffer;
        buffer << package_stream.rdbuf();

        objects::data_type blob(
            buffer.str().data(),
            buffer.str().size()
        );

        try {
            package_t package(context, blob);
            compression = package.type();
        } catch(const package_error_t& e) {
            std::cerr << "Error: the app package in '" << package_path << "' is corrupted." << std::endl;
            std::cerr << e.what() << std::endl;
            return;
        }

        objects::value_type object = { manifest, blob };
       
        std::cout << "Detected app type: '" << type 
                  << "', package compression: '" << compression
                  << "'." << std::endl;

        try {
            context.storage<storages::objects>("core")->put("apps", name, object);
        } catch(const storage_error_t& e) {
            std::cerr << "Error: unable to deploy the app." << std::endl;
            std::cerr << e.what() << std::endl;
            return;
        }

        std::cout << "The '" << name << "' app has been successfully deployed." << std::endl;
    }
}

int main(int argc, char * argv[]) {
    po::options_description general_options("General options"),
                            hidden_options,
                            combined_options;
    
    po::positional_options_description positional_options;
    
    po::variables_map vm;

    general_options.add_options()
        ("help,h", "show this message")
        ("version,v", "show version and build information")
        ("configuration,c", po::value<std::string>
            ()->default_value("/etc/cocaine/cocaine.conf"),
            "location of the configuration file")
        ("manifest,m", po::value<std::string>
            ()->default_value("manifest.json"),
            "location of the app manifest")
        ("package,p", po::value<std::string>
            ()->default_value("package.tar.gz"),
            "location of the app source package")
        ("verbose", "produce a lot of output");

    hidden_options.add_options()
        ("name", po::value<std::string>());

    positional_options.add("name", -1);

    combined_options.add(general_options)
                    .add(hidden_options);

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
        std::cout << "Usage: " << argv[0] << " [options] app-name" << std::endl;
        std::cout << general_options;
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
        std::cerr << "Type '" << argv[0] << " --help' for usage information." << std::endl;
        return EXIT_FAILURE;
    }

    if(!vm.count("manifest")) {
        std::cerr << "Error: no app manifest file location has been specified." << std::endl;
        std::cerr << "Type '" << argv[0] << " --help' for usage information." << std::endl;
        return EXIT_FAILURE;
    }
    
    if(!vm.count("package")) {
        std::cerr << "Error: no app package file location has been specified." << std::endl;
        std::cerr << "Type '" << argv[0] << " --help' for usage information." << std::endl;
        return EXIT_FAILURE;
    }
    
    if(!vm.count("name")) {
        std::cerr << "Error: no app name has been specified." << std::endl;
        std::cerr << "Type '" << argv[0] << " --help' for usage information." << std::endl;
        return EXIT_FAILURE;
    }
    
    // Startup
    // -------

    context_t context(
        vm["configuration"].as<std::string>(),
        boost::make_shared<stdio_sink_t>(
            vm.count("verbose") ? logging::debug : logging::info
        )
    );

    deploy(
        context,
        vm["name"].as<std::string>(),
        vm["manifest"].as<std::string>(),
        vm["package"].as<std::string>()
    );
}

