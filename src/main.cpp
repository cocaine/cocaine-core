#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include "cocaine/common.hpp"
#include "cocaine/core.hpp"
#include "cocaine/helpers/pid_file.hpp"

using namespace cocaine;

namespace po = boost::program_options;
namespace fs = boost::filesystem;

static const char identity[] = "cocaine";

int main(int argc, char* argv[]) {
    po::options_description mandatory, options("Allowed options"), combined;
    po::positional_options_description positional;
    po::variables_map vm;

    mandatory.add_options()
        ("endpoints", po::value< std::vector<std::string> >
            (&config_t::set().core.endpoints));
    
    positional.add("endpoints", -1);

    options.add_options()
        ("core:announce-endpoint", po::value<std::string>
            (&config_t::set().core.announce_endpoint),
            "multicast endpoint for automatic discovery")
        ("core:announce-interval", po::value<float>
            (&config_t::set().core.announce_interval)->default_value(5.0f),
            "multicast announce interval for automatic discovery, seconds")
        ("core:instance", po::value<std::string>
            (&config_t::set().core.instance)->default_value("default"),
            "instance name")
        ("daemonize", "daemonize on start")
        ("engine:backend", po::value<std::string>
            (&config_t::set().engine.backend)->default_value("process"),
            "default engine backend, one of: thread, process")
        ("engine:heartbeat-timeout", po::value<float>
            (&config_t::set().engine.heartbeat_timeout)->default_value(60.0f),
            "default unresponsive thread cancellation timeout, seconds")
        ("engine:suicide-timeout", po::value<float>
            (&config_t::set().engine.suicide_timeout)->default_value(600.0f),
            "default stale thread suicide timeout, seconds")
        ("engine:pool-limit", po::value<unsigned int>
            (&config_t::set().engine.pool_limit)->default_value(10),
            "maximum engine worker pool size")
        ("engine:queue-limit", po::value<unsigned int>
            (&config_t::set().engine.queue_limit)->default_value(10),
            "default maximum engine worker queue depth")
        ("help", "show this message")
        ("pidfile", po::value<fs::path>()->default_value("/var/run/cocaine/default.pid"),
            "location of a pid file")
        ("plugins", po::value<std::string>
            (&config_t::set().registry.location)->default_value("/usr/lib/cocaine"),
            "where to load plugins from")
        ("storage:driver", po::value<std::string>
            (&config_t::set().storage.driver)->default_value("files"),
            "storage driver type, one of: void, files, mongo")
        ("storage:location", po::value<std::string>
            (&config_t::set().storage.location)->default_value("/var/lib/cocaine"),
            "storage location, format depends on the storage type")
        ("verbose", "produce a lot of output");

    combined.add(mandatory).add(options);

    try {
        po::store(
            po::command_line_parser(argc, argv).
                options(combined).
                positional(positional).
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
        std::cout << options;
        return EXIT_SUCCESS;
    }

    if(!vm.count("endpoints")) {
        std::cout << "Error: no endpoints specified" << std::endl;
        std::cout << "Try '" << argv[0] << " --help' for more information" << std::endl;
        return EXIT_FAILURE;
    }

    // Fetching the hostname
    char hostname[256];

    if(gethostname(hostname, 256) == 0) {
        config_t::set().core.hostname = hostname;
    } else {
        std::cout << "Error: failed to determine the hostname" << std::endl;
        return EXIT_FAILURE;
    }

    // Setting up the syslog
    openlog(identity, LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask(LOG_UPTO(vm.count("verbose") ? LOG_DEBUG : LOG_INFO));
    syslog(LOG_NOTICE, "main: blow!");

    // Pid file holder
    std::auto_ptr<helpers::pid_file_t> pidfile;

    // Daemonizing, if needed
    if(vm.count("daemonize")) {
        if(daemon(0, 0) < 0) {
            syslog(LOG_ERR, "main: daemonization failed");
            return EXIT_FAILURE;
        }

        try {
            pidfile.reset(new helpers::pid_file_t(vm["pidfile"].as<fs::path>()));
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "main: %s", e.what());
            return EXIT_FAILURE;
        }
    }
    
    // Cocaine core
    std::auto_ptr<core::core_t> core;

    // Initializing the core
    try {
        core.reset(new core::core_t());
    } catch(const std::exception& e) {
        syslog(LOG_ERR, "main: unable to start the core - %s", e.what());
        return EXIT_FAILURE;
    }

    // This call blocks
    core->start();

    // Cleanup
    core.reset();

    syslog(LOG_NOTICE, "main: terminated");
    
    return EXIT_SUCCESS;
}
