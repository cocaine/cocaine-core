#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "common.hpp"
#include "core.hpp"

using namespace yappi::core;
namespace po = boost::program_options;
namespace fs = boost::filesystem;

static const char identity[] = "yappi";

int main(int argc, char* argv[]) {
    std::string uuid;
    std::vector<std::string> exports;

    po::options_description mandatory, options("Allowed options"), combined;
    po::positional_options_description positional;
    po::variables_map config;

    mandatory.add_options()
        ("listen", po::value< std::vector<std::string> >());
    
    positional.add("listen", -1);

    options.add_options()
        ("help", "show this message")
        ("export", po::value< std::vector<std::string> >(&exports),
            "endpoints to publish events from schedulers")
        ("watermark", po::value<uint64_t>()->default_value(1000),
            "maximum number of messages to keep on client disconnects")
        ("uuid", po::value<fs::path>()->default_value("/var/lib/yappi/default.instance"),
            "force instance uuid file")
        ("pid", po::value<fs::path>()->default_value("/var/run/yappi.pid"),
            "location of a pid file")
        ("daemonize", "daemonize on start")
        ("purge", "purge recovery information");

    combined.add(mandatory).add(options);

    try {
        po::store(
            po::command_line_parser(argc, argv).
                options(combined).
                positional(positional).
                run(),
            config);
        po::notify(config);
    } catch(const po::unknown_option& e) {
        std::cerr << "Error: " << e.what() << "." << std::endl;
    }

    if(config.count("help")) {
        std::cout << "Usage: " << argv[0] << " endpoint-list [options]" << std::endl;
        std::cout << options;
        return EXIT_SUCCESS;
    }

    if(!config.count("listen")) {
        std::cout << "Error: no endpoints specified." << std::endl;
        std::cout << "Try '" << argv[0] << " --help' for more information." << std::endl;
        return EXIT_FAILURE;
    }

    // Setting up the syslog
    openlog(identity, LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask(LOG_UPTO(LOG_DEBUG));
    syslog(LOG_NOTICE, "main: yappi is starting");
        
    // Obtaining the instance uuid
    fs::fstream uuid_file(config["uuid"].as<fs::path>(),
        fs::fstream::in | fs::fstream::out | fs::fstream::app);

    if(!uuid_file) {
        syslog(LOG_ERR, "main: failed to access %s",
            config["uuid"].as<fs::path>().string().c_str());
        return EXIT_FAILURE;
    }

    uuid_file >> uuid;
    
    if(uuid.empty()) {
        syslog(LOG_INFO, "main: first run - generating the instance uuid");
        uuid = yappi::helpers::auto_uuid_t().get();
        uuid_file.clear();
        uuid_file << uuid;
    } else {
        uuid_t dummy;

        // Validate the given uuid
        if(uuid_parse(uuid.c_str(), dummy) == -1) {
            syslog(LOG_ERR, "main: invalid instance uuid - %s", uuid.c_str());
            return EXIT_FAILURE;
        }
    }
    
    uuid_file.close();

    // Daemonizing, if needed
    if(config.count("daemonize")) {
        if(daemon(0, 0) < 0) {
            syslog(LOG_ERR, "main: daemonization failed");
            return EXIT_FAILURE;
        }

        // Write the pidfile
        fs::ofstream pid_file(config["pid"].as<fs::path>(),
            fs::ofstream::out | fs::ofstream::trunc);

        if(!pid_file) {
            syslog(LOG_ERR, "main: failed to write %s",
                config["pid"].as<fs::path>().string().c_str());
            return EXIT_FAILURE;
        }

        pid_file << getpid();
        pid_file.close();
    }

    core_t* core;

    // Initializing the core
    try {
        core = new core_t(
            uuid,
            config["listen"].as< std::vector<std::string> >(),
            exports,
            config["watermark"].as<uint64_t>(),
            config.count("purge"));
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "main: runtime error - %s", e.what());
        return EXIT_FAILURE;
    } catch(const zmq::error_t& e) {
        syslog(LOG_ERR, "main: network error - %s", e.what());
        return EXIT_FAILURE;
    }

    // This call blocks
    core->run();

    // Cleanup
    delete core;

    if(config.count("daemonize")) {
        try {
            fs::remove(config["pid"].as<fs::path>());
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "main: failed to remove %s",
                config["pid"].as<fs::path>().string().c_str());
        }
    }
        
    syslog(LOG_NOTICE, "main: yappi has terminated");
    
    return EXIT_SUCCESS;
}
