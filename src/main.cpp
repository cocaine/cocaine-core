#include <signal.h>

#include "registry.hpp"
#include "core.hpp"

registry_t* theRegistry;
core_t* theCore;

void terminate(int signum) {
    theCore->stop();
};

int main(int argc, char* argv[]) {
    // Setting up the syslog
    openlog(core_t::identity, LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask(LOG_UPTO(LOG_DEBUG));
    syslog(LOG_INFO, "yappi, v%d.%d.%d",
        core_t::version[0], core_t::version[1], core_t::version[2]);

    // Starting
    if(daemon(0, 0) < 0) {
        syslog(LOG_EMERG, "daemonization failed");
        return EXIT_FAILURE;
    } else {
        signal(SIGINT, &terminate);
        signal(SIGTERM, &terminate);

        // TODO: Customize it via argv
        char r_ep[] = "tcp://*:1710";
        char e_ep[] = "tcp://*:1711";
        time_t interval = 2500;
        int64_t watermark = 1000000;
        unsigned int io_threads = 10;

        theRegistry = new registry_t("/home/kobolog/Code/yappi/plugins");
        theCore = new timed_core_t(r_ep, e_ep, watermark, io_threads, interval);
        
        // This call blocks
        theCore->start();

        // Cleanup
        delete theCore;
        delete theRegistry;
    }

    syslog(LOG_INFO, "yappi has terminated");    
    return EXIT_SUCCESS;
}
