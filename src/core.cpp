#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include <signal.h>

#include "core.hpp"
#include "registry.hpp"

using namespace yappi::core;
using namespace yappi::engines;
using namespace yappi::plugins;

const char core_t::identity[] = "yappi";
const int core_t::version[] = { 0, 0, 1 };

extern registry_t* theRegistry;

core_t::core_t(char* ep_req, char* ep_export, int64_t watermark, unsigned int io_threads, time_t interval):
    m_context(io_threads),
    s_requests(m_context, ZMQ_REP),
    s_events(m_context, ZMQ_PULL),
    s_export(m_context, ZMQ_PUB),
    m_interval(interval * 1000),
    m_signal(0)
{
    // Version dump
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);
    syslog(LOG_INFO, "using libzmq v%d.%d.%d",
        major, minor, patch);

    // Argument dump
    syslog(LOG_INFO, "interval: %lu, watermark: %llu events, %d io threads",
        interval, watermark, io_threads);

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
    regcomp(&r_loop, "loop [0-9]+ [0-9]+ [a-z]+://.*", REG_EXTENDED | REG_NOSUB);
    regcomp(&r_unloop, "unloop [a-z]+://[[:alnum:]]{32}", REG_EXTENDED | REG_NOSUB);
    regcomp(&r_once, "once [a-z]+://.*", REG_EXTENDED | REG_NOSUB);
}

core_t::~core_t() {
    syslog(LOG_DEBUG, "shutting down the engines");
    for(engines_t::iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        delete it->second;
    }

    // Destroying regexps
    regfree(&r_loop);
    regfree(&r_unloop);
    regfree(&r_once);
}

void core_t::run() {
    zmq_pollitem_t sockets[2];
    sockets[0].socket = (void*)s_requests;
    sockets[1].socket = (void*)s_events;

    zmq::message_t message;

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
    //     2.3. If it's a one-time request
    //          2.3.1. Obtain the source
    //          2.3.2. Fetch the data
    //          2.3.3. Return the formatted response
    //  3. Loop until there are no requests left in the queue
    //  4. Reap all reapable engines
    //  5. Fetch next event
    //     5.1. If there no engines with the specified id, discard it
    //     5.2. If there is, dissassemle it into fields and publish
    //  6. Loop until there are no events left in the queue
    //  7. Sleep until something happends

    while(true) {
        // 1. Store the current time
        clock_gettime(CLOCK_REALTIME, &m_now);

        // 2. Fetching requests, if any
        if(sockets[0].revents == ZMQ_POLLIN) {
            for(int32_t cnt = 0;; ++cnt) {
                if(!s_requests.recv(&message, ZMQ_NOBLOCK)) {
                    syslog(LOG_DEBUG, "dispatched %d requests", cnt);
                    break;
                }

                // 2.*. Dispatch the request
                std::string request(
                    reinterpret_cast<char*>(message.data()),
                    message.size());
                dispatch(request);
            }
        }

        // 4. Reap all reapable engines
        // It kills those engines whose TTL has expired
        // or whose reference counter has dropped to zero
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

        // 5. Fetching new events, if any
        if(sockets[1].revents == ZMQ_POLLIN) {
            for(int32_t cnt = 0;; ++cnt) {
                if(!s_events.recv(&message, ZMQ_NOBLOCK)) {
                    syslog(LOG_DEBUG, "processed %d events", cnt);
                    break;
                }

                // 5.*. Publish the event
                event_t *event = reinterpret_cast<event_t*>(message.data());
                publish(*event);
                delete event->dict;
            }
        }

        // 7. Sleeping
        // TODO: Make it absolute again
        try {
            sockets[0].events = ZMQ_POLLIN;
            sockets[1].events = ZMQ_POLLIN;
            zmq::poll(sockets, 2, m_interval);
        } catch(const zmq::error_t& e) {
            if(e.num() == EINTR && (m_signal == SIGINT || m_signal == SIGTERM)) {
                // Got termination signal, so stop the loop
                break;
            } else {
                syslog(LOG_ERR, "something bad happened: %s", e.what());
                break;
            }
        }
    }
}

void core_t::publish(const event_t& event) {
    zmq::message_t message;
        
    if(m_engines.find(event.key) == m_engines.end()) {
        // 5.1. Outstanding event from a stopped engine
        syslog(LOG_DEBUG, "discarding event for %s", event.key.c_str());
        return;
    }

    // 5.2. Disassemble and publish
    for(dict_t::iterator it = event.dict->begin(); it != event.dict->end(); ++it) {
        std::ostringstream fmt;
        fmt << event.key << " " << it->first << "=" << it->second << " @" << m_now.tv_sec;
    
        message.rebuild(fmt.str().length());
        memcpy(message.data(), fmt.str().data(), fmt.str().length());
        s_export.send(message);    
    }
}

// Message types:
// --------------
// * Loop - launches a thread which fetches data from the
//   specified source and publishes it via the PUB socket. Plugin
//   will be invoked every 'timeout' microsecods, for 'ttl' seconds
//   or forever if ttl = 0:
//   -> loop interval-in-ms ttl-in-s source://parameters
//   <- key|e source|e arguments|e runtime
//
// * Unloop - shuts down the specified thread.
//   Remaining messages will stay orphaned in the queue,
//   so it's a good idea to drain it after the unsubscription:
//   -> unloop key
//   <- ok|e key
//
// * Once - one-time plugin invocation. For one-time invocations,
//   you have no means to filter the data by categories or fields:
//   -> once source://parameters
//   <- field=value field2=value2... @timestamp|e source
//
// * [!] History - fetch historical data without plugin invocation. 
//   You can't fetch more messages than there were invocations:
//   -> history depth source://parameters
//   <- n-part once-like message, where n = max(depth, history-length)
//
// Publishing format:
// ------------------
//   key field=value @timestamp

void core_t::dispatch(const std::string& request) {
    std::string cmd;
    std::istringstream fmt(request);

    // NOTE: This is unused for now, but might come in handy
    // If I decide to drop regex and use something else
    // for command validation
    fmt >> std::skipws >> cmd;

    if(regexec(&r_loop, request.c_str(), 0, 0, 0) == 0) {
        // 2.1. Loop
        std::string uri;
        time_t interval, ttl;
        
        fmt >> interval >> ttl >> uri;
        loop(uri, interval, ttl);
   
        return;
    }

    if(regexec(&r_unloop, request.c_str(), 0, 0, 0) == 0) {
        // 2.2. Unloop
        std::string key;
        
        fmt >> key;
        unloop(key);
        
        return;
    }

    if(regexec(&r_once, request.c_str(), 0, 0, 0) == 0) {
        // 2.3. Once
        std::string uri;

        fmt >> uri;
        once(uri);

        return;
    }

    syslog(LOG_ERR, "got an unsupported request: %s", request.c_str());
    respond("e request");
}

void core_t::loop(const std::string& uri, time_t interval, time_t ttl) {
    // 2.1.1. Generating the key
    std::string scheme(uri.substr(0, uri.find_first_of(':')));
    std::string key = scheme + "://" + m_keygen.get(uri);
    
    // Validating TTL. It's meaningless to set it shorter than the core interval
    if(ttl < m_interval / 1000) {
        syslog(LOG_WARNING, "ttl requested is less than core interval");
        ttl = m_interval / 1000;
    }
    
    // 2.1.2. Search for the engine
    engines_t::iterator it = m_engines.find(key);

    if(it != m_engines.end()) {
        // 2.1.3. Increment reference counter and update timers
        it->second->subscribe(interval, ttl);
    } else {
        // 2.1.4. Start a new engine
        try {
            loop_t* engine = new loop_t(key, theRegistry->create(scheme, uri), m_context, interval, ttl);
            m_engines.insert(std::make_pair(key, engine));
            syslog(LOG_DEBUG, "created a new engine with uri: %s, interval: %lu, ttl: %lu",
                uri.c_str(), interval, ttl);
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "engine creation failed: %s", e.what());
            respond("e runtime");
            return;
        } catch(const std::invalid_argument& e) {
            syslog(LOG_ERR, "invalid uri: %s", e.what());
            respond("e arguments");
            return;
        } catch(const std::domain_error& e) {
            syslog(LOG_ERR, "unknown source type: %s", e.what());
            respond("e source");
            return;
        }
    }

    // 2.1.5. Return the key
    respond(key);
}

void core_t::unloop(const std::string& key) {
    // 2.2.1. Search for the engine 
    engines_t::iterator it = m_engines.find(key);
    
    if(it == m_engines.end()) {
        syslog(LOG_ERR, "got an invalid key: %s", key.c_str());
        respond("e key");
        return;
    }

    // 2.2.2. Decrement reference counter
    it->second->unsubscribe();
    respond("ok");
}

void core_t::once(const std::string& uri) {
    std::string scheme(uri.substr(0, uri.find_first_of(':')));
    std::ostringstream fmt;
    source_t* source;
        
    try {
        // 2.3.1. Create a new source
        source = theRegistry->create(scheme, uri);
    } catch(const std::invalid_argument& e) {
        syslog(LOG_ERR, "invalid uri: %s", e.what());
        respond("e arguments");
        return;
    } catch(const std::domain_error& e) {
        syslog(LOG_ERR, "unknown source type: %s", e.what());
        respond("e source");
        return;
    }

    // 2.3.2. Fetch the data once
    dict_t dict = source->fetch();
    
    // Cleanup
    delete source;

    // Prepare the response
    for(dict_t::iterator it = dict.begin(); it != dict.end(); ++it) {
        fmt << it->first << "=" << it->second << " ";    
    }

    fmt << "@" << m_now.tv_sec;
    
    // 2.3.3. Send it away
    respond(fmt.str());
}

void core_t::respond(const std::string& response) {
    zmq::message_t message(response.length());
    memcpy(message.data(), response.data(), response.length()); 

    s_requests.send(message, ZMQ_NOBLOCK);
}
