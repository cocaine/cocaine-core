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
const char core_t::version[] = "version 0.0.1";

extern registry_t* theRegistry;

core_t::core_t(const std::vector<std::string>& listen_eps, const std::vector<std::string>& export_eps,
               int64_t watermark, unsigned int threads, time_t interval):
    m_context(threads),
    s_events(m_context, ZMQ_PULL),
    s_listen(m_context, ZMQ_REP),
    s_export(m_context, ZMQ_PUB),
    m_signal(0),
    m_interval(interval * 1000)
{
    // Version dump
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);
    syslog(LOG_INFO, "using libzmq version %d.%d.%d",
        major, minor, patch);

    // Argument dump
    syslog(LOG_INFO, "poll timeout: %lums, watermark: %d events, %d threads",
        interval, static_cast<int32_t>(watermark), threads);

    // Binding request endpoints
    if(!listen_eps.size()) {
        throw std::runtime_error("no listen endpoints specified");
    }
    
    for(std::vector<std::string>::const_iterator it = listen_eps.begin(); it != listen_eps.end(); ++it) {
        s_listen.bind(it->c_str());
        syslog(LOG_INFO, "listening on %s", it->c_str());
    }

    // Binding export endpoints
    if(!export_eps.size()) {
        syslog(LOG_WARNING, "no export endpoints specified, loop/unloop commands will be useless");
    } else {
        s_export.setsockopt(ZMQ_HWM, &watermark, sizeof(watermark));
        for(std::vector<std::string>::const_iterator it = export_eps.begin(); it != export_eps.end(); ++it) {
            s_export.bind(it->c_str());
            syslog(LOG_INFO, "exporting on %s", it->c_str());
        }
    }

    // Binding event collection endpoint
    s_events.setsockopt(ZMQ_HWM, &watermark, sizeof(watermark));
    s_events.bind("inproc://events");

    // Initializing regexps
    regcomp(&r_loop, "loop [0-9]+ [0-9]+ [a-z]+://.*", REG_EXTENDED | REG_NOSUB);
    regcomp(&r_unloop, "unloop [a-z]+://[[:alnum:]]{16}", REG_EXTENDED | REG_NOSUB);
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
    zmq_pollitem_t sockets[] = {
        { (void*)s_listen, 0, ZMQ_POLLIN, 0 },
        { (void*)s_events, 0, ZMQ_POLLIN, 0 }
    };

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
        // 7. Sleep
        try {
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

        // 1. Store the current time
        clock_gettime(CLOCK_REALTIME, &m_now);
        
        // 2. Fetching requests, if any
        if(sockets[0].revents & ZMQ_POLLIN) {
            while(s_listen.recv(&message, ZMQ_NOBLOCK)) {
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
        if(sockets[1].revents & ZMQ_POLLIN) {
            while(s_events.recv(&message, ZMQ_NOBLOCK)) {
                // 5.*. Publish the event
                event_t *event = reinterpret_cast<event_t*>(message.data());
                publish(*event);
                delete event->dict;
            }
        }
    } // Loop
}

void core_t::publish(const event_t& event) {
    zmq::message_t message;
        
    if(m_engines.find(event.key) == m_engines.end()) {
        // 5.1. Outstanding event from a stopped engine
        syslog(LOG_DEBUG, "discarding event for %s", event.key.c_str());
        return;
    }

    // 5.2. Disassemble and publish in the envelope
    for(dict_t::iterator it = event.dict->begin(); it != event.dict->end(); ++it) {
        std::ostringstream envelope;
        
        envelope << event.key << " " << it->first << " @" << (m_now.tv_sec * 1000 + m_now.tv_nsec / 1e+6);
        message.rebuild(envelope.str().length());
        memcpy(message.data(), envelope.str().data(), envelope.str().length());
        s_export.send(message, ZMQ_SNDMORE);

        message.rebuild(it->second.length());
        memcpy(message.data(), it->second.data(), it->second.length());
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
    delete source;

    // Prepare the response
    std::ostringstream response;
    
    for(dict_t::iterator it = dict.begin(); it != dict.end(); ++it) {
        response << it->first << "=" << it->second << " ";    
    }

    response << "@" << m_now.tv_sec;
    
    // 2.3.3. Send it away
    respond(response.str());
}

void core_t::respond(const std::string& response) {
    zmq::message_t message(response.length());
    memcpy(message.data(), response.data(), response.length()); 

    s_listen.send(message, ZMQ_NOBLOCK);
}
