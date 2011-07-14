#include <iostream>
#include <fstream>
#include <stdexcept>

#include "common.hpp"
#include "future.hpp"
#include "core.hpp"

using namespace yappi::core;

void usage() {
        std::cout << "Usage: yappi -d -l endpoint -e endpoint [-m 0] [-s 0] [-f /var/run/yappi.pid]" << std::endl;
        std::cout << "  -d\tdaemonize" << std::endl;
        std::cout << "  -l\tendpoint for listening for requests, might be used multiple times" << std::endl;
        std::cout << "  -e\tendpoint for exporting events, might be used multiple times" << std::endl;
        std::cout << "  -m\tin-memory message cache, in messages (0 - unlimited)" << std::endl;
        std::cout << "  -s\ton-disk message cache, in bytes (0 - off)" << std::endl;
        std::cout << "  -f\tpidfile location" << std::endl;
        std::cout << std::endl;
        std::cout << "Endpoint types:" << std::endl;
        std::cout << "  * ipc://pathname" << std::endl;
        std::cout << "  * tcp://(name|address|*):port" << std::endl;
        std::cout << "  * epgm://(name|address);multicast-address:port [export only]" << std::endl;
}

int main(int argc, char* argv[]) {
    char option = 0;

    bool daemonize = false;
    std::string pidfile = "/var/run/yappi.pid";
    std::vector<std::string> listeners;
    std::vector<std::string> publishers;
    uint64_t hwm = 0;
    int64_t swap = 0;

    while((option = getopt(argc, argv, "de:l:p:h:m:s:f:")) != -1) {
        switch(option) {
            case 'd':
                daemonize = true;
                break;
            case 'l':
                listeners.push_back(optarg);
                break;
            case 'e':
                publishers.push_back(optarg);
                break;
            case 'm':
                hwm = atoi(optarg);
                break;
            case 's':
                swap = atoi(optarg);
                break;
            case 'f':
                pidfile = optarg;
                break;
            case 'h':
            default:
                usage();
                return EXIT_SUCCESS;
        }
    }

    if(!listeners.size() || !publishers.size()) {
        std::cerr << "Invalid arguments" << std::endl;
        usage();

        return EXIT_FAILURE;
    }

    // Daemonizing, if needed
    if(daemonize) {
        if(daemon(0, 0) < 0) {
            syslog(LOG_EMERG, "daemonization failed");
            return EXIT_FAILURE;
        }

        // Setting up the syslog
        openlog(core_t::identity, LOG_PID | LOG_NDELAY, LOG_USER);
        setlogmask(LOG_UPTO(LOG_DEBUG));
        
        // Write the pidfile
        std::ofstream file;
        file.exceptions(std::ofstream::badbit | std::ofstream::failbit);

        try {
            file.open(pidfile.c_str(), std::ofstream::out | std::ofstream::trunc);
        } catch(const std::ofstream::failure& e) {
            syslog(LOG_ERR, "failed to write %s", pidfile.c_str());
            return EXIT_FAILURE;
        }     

        file << getpid();
        file.close();
    }

    syslog(LOG_INFO, "yappi is starting");
    core_t* core;

    // Initializing the core
    try {
        core = new core_t(listeners, publishers, hwm, swap);
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "runtime error: %s", e.what());
        return EXIT_FAILURE;
    } catch(const zmq::error_t& e) {
        syslog(LOG_ERR, "network error: %s", e.what());
        return EXIT_FAILURE;
    }

    // This call blocks
    core->run();

    // Cleanup
    delete core;
    std::remove(pidfile.c_str());

    syslog(LOG_INFO, "yappi has terminated");    
    return EXIT_SUCCESS;
}
