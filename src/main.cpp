#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "common.hpp"
#include "core.hpp"

using namespace yappi;
using namespace yappi::core;

namespace po = boost::program_options;
namespace fs = boost::filesystem;

static const char identity[] = "yappi";

int main(int argc, char* argv[]) {
    config_t config;

    po::options_description mandatory, options("Allowed options"), combined;
    po::positional_options_description positional;
    po::variables_map vm;

    mandatory.add_options()
        ("listen", po::value< std::vector<std::string> >
            (&config.net.listen));
    
    positional.add("listen", -1);

    options.add_options()
        ("help", "show this message")
        ("export", po::value< std::vector<std::string> >
            (&config.net.publish),
            "endpoints to publish events from schedulers")
        ("watermark", po::value<uint64_t>
            (&config.net.watermark)->default_value(1000),
            "maximum number of messages to keep on client disconnects")
        ("storage", po::value<std::string>
            (&config.paths.storage)->default_value("/var/lib/yappi/default"),
            "storage path")
        ("plugins", po::value<std::string>
            (&config.paths.plugins)->default_value("/usr/lib/yappi"),
            "plugin path")
        ("pid", po::value<fs::path>()->default_value("/var/run/yappi.pid"),
            "location of a pid file")
        ("thread-suicide-timeout", po::value<float>
            (&config.engine.suicide_timeout)->default_value(600.0),
            "stale thread suicide timeout, in seconds")
        ("thread-collect-timeout", po::value<float>
            (&config.engine.collect_timeout)->default_value(0.5),
            "driver events collection timeout, in seconds")
        ("daemonize", "daemonize on start")
        ("transient", "disable storage completely");

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
        std::cerr << "Error: " << e.what() << "." << std::endl;
    }

    if(vm.count("help")) {
        std::cout << "Usage: " << argv[0] << " endpoint-list [options]" << std::endl;
        std::cout << options;
        return EXIT_SUCCESS;
    }

    if(!vm.count("listen") || !vm.count("export")) {
        std::cout << "Error: no listen/export endpoints specified." << std::endl;
        std::cout << "Try '" << argv[0] << " --help' for more information." << std::endl;
        return EXIT_FAILURE;
    }

    config.storage.disabled = vm.count("transient");

    // Setting up the syslog
    openlog(identity, LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask(LOG_UPTO(LOG_DEBUG));
    syslog(LOG_NOTICE, "main: yappi is starting");
        
    // Daemonizing, if needed
    if(vm.count("daemonize")) {
        if(daemon(0, 0) < 0) {
            syslog(LOG_ERR, "main: daemonization failed");
            return EXIT_FAILURE;
        }

        // Write the pidfile
        fs::ofstream pid_file(vm["pid"].as<fs::path>(),
            fs::ofstream::out | fs::ofstream::trunc);

        if(!pid_file) {
            syslog(LOG_ERR, "main: failed to write %s",
                vm["pid"].as<fs::path>().string().c_str());
            return EXIT_FAILURE;
        }

        pid_file << getpid();
        pid_file.close();
    }

    core_t* core;

    // Initializing the core
    try {
        core = new core_t(config);
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
    delete core;

    if(vm.count("daemonize")) {
        try {
            fs::remove(vm["pid"].as<fs::path>());
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "main: failed to remove %s",
                vm["pid"].as<fs::path>().string().c_str());
        }
    }
        
    syslog(LOG_NOTICE, "main: yappi has terminated");
    
    return EXIT_SUCCESS;
}
