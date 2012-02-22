//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#include <stdarg.h>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include "cocaine/config.hpp"
#include "cocaine/context.hpp"
#include "cocaine/common.hpp"
#include "cocaine/core.hpp"
#include "cocaine/helpers/pid_file.hpp"

using namespace cocaine;

namespace po = boost::program_options;
namespace fs = boost::filesystem;

class syslog_t:
    public logger_t
{
    public:
        syslog_t(const std::string& identity, int verbosity):
            m_identity(identity)
        {
            // Setting up the syslog
            openlog(m_identity.c_str(), LOG_PID | LOG_NDELAY, LOG_USER);
            setlogmask(LOG_UPTO(verbosity));
        }

    public:
        virtual void emit(int priority, const char* format, ...) {
            va_list args;
            va_start(args, format);
            syslog(priority, format, args);
            va_end(args);
        }

    private:
        const std::string m_identity;
};

int main(int argc, char* argv[]) {
    config_t config;

    po::options_description
        hidden_options,
        general_options("General options"),
        core_options("Core options"),
        engine_options("Engine options"),
        storage_options("Storage options"),
        combined_options;
    
    po::positional_options_description positional_options;
    po::variables_map vm;

    hidden_options.add_options()
        ("endpoints", po::value< std::vector<std::string> >
            (&config.core.endpoints)->composing(),
            "core endpoints for server management");
    
    positional_options.add("endpoints", -1);

    general_options.add_options()
        ("help,h", "show this message")
        ("version,v", "show version and build information")
        ("daemonize", "daemonize on start")
        ("pidfile", po::value<fs::path>
            ()->default_value("/var/run/cocaine/default.pid"),
            "location of a pid file")
        ("verbose", "produce a lot of output");

    core_options.add_options()
        ("core:instance", po::value<std::string>
            (&config.core.instance)->default_value("default"),
            "instance name")
        ("core:plugins", po::value<std::string>
            (&config.core.plugins)->default_value("/usr/lib/cocaine"),
            "where to load plugins from")
        ("core:announce-endpoint", po::value<std::string>
            (&config.core.announce_endpoint),
            "multicast endpoint for automatic discovery")
        ("core:announce-interval", po::value<float>
            (&config.core.announce_interval)->default_value(5.0f),
            "multicast announce interval for automatic discovery, seconds");

    engine_options.add_options()
        ("engine:backend", po::value<std::string>
            (&config.engine.backend)->default_value("process"),
            "default engine backend, one of: thread, process")
        ("engine:heartbeat-timeout", po::value<float>
            (&config.engine.heartbeat_timeout)->default_value(30.0f),
            "default unresponsive thread cancellation timeout, seconds")
        ("engine:suicide-timeout", po::value<float>
            (&config.engine.suicide_timeout)->default_value(600.0f),
            "default stale thread suicide timeout, seconds")
        ("engine:pool-limit", po::value<unsigned int>
            (&config.engine.pool_limit)->default_value(10),
            "maximum engine slave pool size")
        ("engine:queue-limit", po::value<unsigned int>
            (&config.engine.queue_limit)->default_value(10),
            "default maximum engine queue depth");

    storage_options.add_options()
        ("storage:driver", po::value<std::string>
            (&config.storage.driver)->default_value("files"),
            "storage driver type, one of: void, files, mongo")
        ("storage:location", po::value<std::string>
            (&config.storage.location)->default_value("/var/lib/cocaine"),
            "storage location, format depends on the storage type");

    combined_options.add(hidden_options)
                    .add(general_options)
                    .add(core_options)
                    .add(engine_options)
                    .add(storage_options);

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
        std::cout << general_options << core_options << engine_options << storage_options;
        return EXIT_SUCCESS;
    }

    if(vm.count("version")) {
        std::cout << "Cocaine " << COCAINE_VERSION << std::endl;
        return EXIT_SUCCESS;
    }
    
    // Pid file holder
    std::auto_ptr<helpers::pid_file_t> pidfile;

    // Setting up the logging facility
    boost::shared_ptr<syslog_t> log(
        new syslog_t(
            "cocaine",
            vm.count("verbose") ? LOG_DEBUG : LOG_INFO
        )
    );

    // Daemonizing, if needed
    if(vm.count("daemonize")) {
        if(daemon(0, 0) < 0) {
            log->emit(LOG_ERR, "main: daemonization failed");
            return EXIT_FAILURE;
        }

        try {
            pidfile.reset(new helpers::pid_file_t(vm["pidfile"].as<fs::path>()));
        } catch(const std::runtime_error& e) {
            log->emit(LOG_ERR, "main: %s", e.what());
            return EXIT_FAILURE;
        }
    }
    
    // Cocaine core
    std::auto_ptr<core::core_t> core;

    // Initializing the core
    try {
        core.reset(new core::core_t(config, log));
    } catch(const std::exception& e) {
        log->emit(LOG_ERR, "main: unable to start the core - %s", e.what());
        return EXIT_FAILURE;
    }

    core->loop();

    // Cleanup
    core.reset();

    log->emit(LOG_NOTICE, "main: terminated");
    
    return EXIT_SUCCESS;
}
