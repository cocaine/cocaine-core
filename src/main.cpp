#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "cocaine/common.hpp"
#include "cocaine/core.hpp"
#include "cocaine/helpers/pid_file.hpp"

using namespace cocaine;
using namespace cocaine::core;
using namespace cocaine::helpers;

namespace po = boost::program_options;
namespace fs = boost::filesystem;

static const char identity[] = "cocaine";

int main(int argc, char* argv[]) {
    po::options_description mandatory, options("Allowed options"), combined;
    po::positional_options_description positional;
    po::variables_map vm;

    mandatory.add_options()
        ("listen", po::value< std::vector<std::string> >
            (&config_t::set().net.listen));
    
    positional.add("listen", -1);

    options.add_options()
        ("help", "show this message")
        ("instance", po::value<std::string>
            (&config_t::set().core.instance)->default_value("default"),
            "instance name")
        ("storage-driver", po::value<std::string>
            (&config_t::set().storage.driver)->default_value("files"),
            "storage driver, one of: void, files, mongo")
        ("storage-location", po::value<std::string>
            (&config_t::set().storage.location)->default_value("/var/lib/cocaine"),
            "storage location, format depends on the storage type")
        ("plugins", po::value<std::string>
            (&config_t::set().registry.location)->default_value("/usr/lib/cocaine"),
            "plugin path")
        ("pidfile", po::value<fs::path>()->default_value("/var/run/cocaine.pid"),
            "location of a pid file")
        ("engine-pool-limit", po::value<unsigned int>
            (&config_t::set().engine.pool_limit)->default_value(10),
            "maximum engine thread pool size")
        ("engine-history-depth", po::value<unsigned int>
            (&config_t::set().engine.history_depth)->default_value(10),
            "maximum history depth for tasks")
        ("thread-collect-timeout", po::value<float>
            (&config_t::set().engine.collect_timeout)->default_value(0.5),
            "driver events collection timeout, seconds")
        ("thread-suicide-timeout", po::value<float>
            (&config_t::set().engine.suicide_timeout)->default_value(600.0),
            "stale thread suicide timeout, seconds")
        ("thread-heartbeat-timeout", po::value<float>
            (&config_t::set().engine.heartbeat_timeout)->default_value(60.0),
            "unresponsive thread cancellation timeout, seconds")
        ("thread-queue-depth", po::value<unsigned int>
            (&config_t::set().engine.queue_depth)->default_value(10),
            "engine's thread queue depth")
        ("daemonize", "daemonize on start")
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

    if(!vm.count("listen")) {
        std::cout << "Error: no listen endpoints specified" << std::endl;
        std::cout << "Try '" << argv[0] << " --help' for more information" << std::endl;
        return EXIT_FAILURE;
    }

    // Setting up the syslog
    openlog(identity, LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask(LOG_UPTO(vm.count("verbose") ? LOG_DEBUG : LOG_INFO));
    syslog(LOG_NOTICE, "main: blow");

    // Pid file holder
    std::auto_ptr<pid_file_t> pidfile;

    // Daemonizing, if needed
    if(vm.count("daemonize")) {
        if(daemon(0, 0) < 0) {
            syslog(LOG_ERR, "main: daemonization failed");
            return EXIT_FAILURE;
        }

        try {
            pidfile.reset(new pid_file_t(vm["pidfile"].as<fs::path>()));
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "main: %s", e.what());
            return EXIT_FAILURE;
        }
    }
    
    // Cocaine core
    boost::shared_ptr<core_t> core;

    // Initializing the core
    try {
        core.reset(new core_t());
    } catch(const zmq::error_t& e) {
        syslog(LOG_ERR, "main: network error - %s", e.what());
        return EXIT_FAILURE;
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "main: runtime error - %s", e.what());
        return EXIT_FAILURE;
    }

    // This call blocks
    core->run();

    // Cleanup
    core.reset();

    syslog(LOG_NOTICE, "main: exhale");
    
    return EXIT_SUCCESS;
}
