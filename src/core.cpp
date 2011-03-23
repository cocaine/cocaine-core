#include <map>
#include <sstream>
#include <boost/tuple/tuple.hpp>

#include <cstdlib>
#include <signal.h>

#include "core.hpp"

// Temporary
#include "mysql.hpp"

const char core_t::identity[] = "yappi";
const int core_t::version[] = { 0, 0, 1 };

core_t::core_t(char* ep_req, char* ep_export, time_t interval, int64_t watermark, unsigned int io_threads):
    m_watermark(watermark),
    m_context(io_threads),
    s_requests(m_context, ZMQ_REP),
    s_events(m_context, ZMQ_PULL),
    s_export(m_context, ZMQ_PUB),
    m_running(false)
{
    // Version dump
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);
    syslog(LOG_INFO, "using libzmq v%d.%d.%d",
        major, minor, patch);

    // Argument dump
    syslog(LOG_INFO, "interval: %ldms, watermark: %llu events, %d io",
        interval, watermark, io_threads);

    clock_parse(interval, m_interval);

    // Socket for requests
    try {
        s_requests.bind(ep_req);
        syslog(LOG_INFO, "listening on %s", ep_req);
    } catch(const zmq::error_t& e) {
        syslog(LOG_EMERG, "cannot bind to %s: %s", ep_req, e.what());
        exit(EXIT_FAILURE);
    }
    
    // Socket for exporting data
    try {
        s_export.bind(ep_export);
        syslog(LOG_INFO, "exporting on %s", ep_export);
    } catch(const zmq::error_t& e) {
        syslog(LOG_EMERG, "cannot bind to %s: %s", ep_export, e.what());
        exit(EXIT_FAILURE);
    }

    // Socket for event collection
    s_events.bind("inproc://events");
    s_events.setsockopt(ZMQ_HWM, &watermark, sizeof(watermark));

    // Initializing regexps
    regcomp(&r_subscribe, "s ([0-9]+) ([0-9]+) ([a-z]+)://(.*)", REG_EXTENDED | REG_NOSUB);
    regcomp(&r_unsubscribe, "u ([[:alnum:]]{32})", REG_EXTENDED | REG_NOSUB);
}

core_t::~core_t() {
    syslog(LOG_DEBUG, "shutting down the engines");
    for(engines_t::iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        delete it->second;
    }

    // Destroying regexps
    regfree(&r_subscribe);
    regfree(&r_unsubscribe);
}

// Message types:
// --------------
// * Subscribe - launches a thread which fetches data from the
//   specified source and publishes it via the PUB socket. Plugin
//   will be invoked every 'timeout' microsecods, for 'ttl' seconds
//   or forever if ttl = 0:
//   -> subscribe[timeout;ttl;scheme://parameters]
//   <- key|failure
//
// * Unsubscribe - shuts down the specified thread.
//   Remaining messages will stay orphaned in the queue,
//   so it's a good idea to drain it after the unsubscription:
//   -> unsubscribe[key]
//   <- success|failure
//
// * Once - one-time plugin invocation. For one-time invocations,
//   you have no means to filter the data by categories or fields:
//   -> once[scheme://parameters]
//   <- message|failure
//
// * History - fetch historical data without plugin invocation. 
//   You can't fetch more messages than there were invocations:
//   -> history[depth;scheme://parameters]
//   <- n-part message, where n = max(depth, history-length)
//
// Publishing format:
// ------------------
//   key:category:field:value
//
//   The following format allows for subscription to all fields
//   of the plugin data, some fields or only one field.

void core_t::run() {
    zmq::message_t message;
    m_running = true;

    // Loop structure:
    // ---------------
    //  1. Store the current time
    //  2. Fetch next request
    //     2.1. If it's a subscribtion:
    //          2.1.1. Generate the key
    //          2.1.2. Check if there is an active engine for the key
    //          2.1.3. If there is, adjust interval and ttl with max(old, new)
    //                 and increment reference counter by one
    //          2.1.4. If there is not, launch a new one
    //          2.1.5. Return the key
    //     2.2. If it's an unsubscription:
    //          2.2.1. Check if there is an active engine for the key
    //          2.2.2. If there is, decrement its reference counter by one
    //  3. Loop until there are no requests left in the queue
    //  4. Reap all reapable engines
    //  5. Fetch next event
    //     4.1. If there no engines with the specified id, discard it
    //     4.2. If there is, dissassemle it into fields and append them to pending list
    //  6. Loop until there are no events left in the queue, or watermark is hit
    //  7. Publish everything from the pending list
    //  8. Sleep until stored time + interval

    while(m_running) {
        // 1. Store the current time
        syslog(LOG_DEBUG, "--- iteration ---");
        clock_gettime(CLOCK_REALTIME, &m_now);

        for(int32_t cnt = 0;; ++cnt) {
            // 2. Try to fetch the next request
            if(!s_requests.recv(&message, ZMQ_NOBLOCK)) {
                if(cnt)
                    syslog(LOG_DEBUG, "dispatched %d requests", cnt);
                break;
            }

            // 2.*. Dispatch the request
            std::string request(
                reinterpret_cast<char*>(message.data()),
                message.size());
            dispatch(request);
        }

        // 4. Reap all reapable engines
        engines_t::iterator it = m_engines.begin();
        while(it != m_engines.end()) {
            if(it->second->reapable(m_now)) {
                syslog(LOG_DEBUG, "reaping engine %s", it->first.c_str());
                delete it->second;
                m_engines.erase(it++);
            } else {
                ++it;
            }
        }

        for(int32_t cnt = 0; cnt < m_watermark; ++cnt) {
            // 5. Try to fetch the next event
            if(!s_events.recv(&message, ZMQ_NOBLOCK)) {
                if(cnt)
                    syslog(LOG_DEBUG, "processed %d events", cnt);
                break;
            }

            // 5.*. Process the event
            event_t* event = reinterpret_cast<event_t*>(message.data());
            feed(*event);
        }

        // 7. Publish
        if(m_pending.size())
            syslog(LOG_DEBUG, "publishing %d items", m_pending.size());
        
        for(pending_t::iterator it = m_pending.begin(); it != m_pending.end(); ++it) {
            message.rebuild(it->length());
            memcpy(message.data(), it->data(), it->length());
            s_export.send(message);
        }

        m_pending.clear();

        // 8. Sleeping
        clock_advance(m_now, m_interval);
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &m_now, NULL);
    }
}

void core_t::dispatch(const std::string& request) {
    std::string cmd, uri;
    std::istringstream fmt(request);

    fmt >> std::skipws >> cmd;

    if(regexec(&r_subscribe, request.c_str(), 0, 0, 0) == 0) {
        // 2.1. Subscription
        time_t interval, ttl;
        
        fmt >> interval >> ttl >> uri;
        subscribe(uri, interval, ttl);
   
        return;
    }

    if(regexec(&r_unsubscribe, request.c_str(), 0, 0, 0) == 0) {
        // 2.2. Unsubscription
        fmt >> uri;
        unsubscribe(uri);
        
        return;
    }

    syslog(LOG_ERR, "got an unsupported request: %s", request.c_str());
    respond("e request");
}

void core_t::subscribe(const std::string& uri, time_t interval, time_t ttl) {
    // 3.2. Search for the engine
    engines_t::iterator it = m_engines.find(uri);
    engine_t* engine;

    if(it != m_engines.end()) {
        // 3.3a. Increment reference counter and update timers
        engine = it->second;
        engine->subscribe(interval, ttl);
    } else {
        // 3.3b. Start a new engine
        try {
            engine = new engine_t(uri, new mysql_t(uri), m_context, interval, ttl);
        } catch(const std::exception&) {
            respond("e resources");
            return;
        }
        
        syslog(LOG_DEBUG, "created a new engine with uri: %s, interval: %lu, ttl: %lu",
            uri.c_str(), interval, ttl);
        m_engines.insert(std::make_pair(uri, engine));
    }

    // 3.4. Return the key
    respond("ok");
}

void core_t::unsubscribe(const std::string& uri) {
    // 4.1. Search for the engine 
    engines_t::iterator it = m_engines.find(uri);
    if(it == m_engines.end()) {
        syslog(LOG_ERR, "got an invalid uri: %s", uri.c_str());
        respond("e uri");
        return;
    }

    // 4.2. Decrement reference counter
    engine_t* engine = it->second;
    engine->unsubscribe();

    respond("ok");
}

void core_t::respond(const std::string& response) {
    zmq::message_t message(response.length());
    memcpy(message.data(), response.data(), response.length()); 

    s_requests.send(message, ZMQ_NOBLOCK);
}

void core_t::feed(event_t& event) {
    if(m_engines.find(event.uri) == m_engines.end()) {
        // 5.1. Outstanding event from a stopped engine
        syslog(LOG_DEBUG, "discarding event for %s", event.uri.c_str());
        delete event.dict;
        return;
    }

    // 6. Disassemble
    for(dict_t::iterator it = event.dict->begin(); it != event.dict->end(); ++it) {
        std::ostringstream fmt;
        fmt << event.uri << " " << it->first << "=" << it->second << " @" << m_now.tv_sec;
        m_pending.push_back(fmt.str());
    }

    delete event.dict;
}

core_t* theCore;

void terminate(int signum) {
    syslog(LOG_INFO, "terminating");
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
        time_t interval = 5000;
        int64_t watermark = 100;
        unsigned int io_threads = 10;

        theCore = new core_t(r_ep, e_ep, interval, watermark, io_threads);
        // This call blocks
        theCore->run();
        delete theCore;
    }

    syslog(LOG_INFO, "kkthxbai");    
    return EXIT_SUCCESS;
}
