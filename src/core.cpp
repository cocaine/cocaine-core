#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include <signal.h>

#include "core.hpp"

using namespace yappi::core;
using namespace yappi::engines;
using namespace yappi::plugins;

const char core_t::identity[] = "yappi";
const char core_t::version[] = "version 0.0.1";

#define TIMESTAMP " @"                      \
    << m_now.tv_sec << "."                  \
    << std::setw(3) << std::setfill('0')    \
    << m_now.tv_nsec / 1000000

core_t::core_t(const std::vector<std::string>& ctl_eps, const std::vector<std::string>& pub_eps,
               const std::string& path, int64_t watermark, unsigned int threads, time_t interval):
    m_registry(path),
    m_context(threads),
    s_events(m_context, ZMQ_PULL),
    s_ctl(m_context, ZMQ_REP),
    s_pub(m_context, ZMQ_PUB),
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
    if(!ctl_eps.size()) {
        throw std::runtime_error("no controlling endpoints specified");
    }
    
    for(std::vector<std::string>::const_iterator it = ctl_eps.begin(); it != ctl_eps.end(); ++it) {
        s_ctl.bind(it->c_str());
        syslog(LOG_INFO, "listening on %s", it->c_str());
    }

    // Binding export endpoints
    if(!pub_eps.size()) {
        throw std::runtime_error("no publishing endpoints specified");
    }

    s_pub.setsockopt(ZMQ_HWM, &watermark, sizeof(watermark));
    for(std::vector<std::string>::const_iterator it = pub_eps.begin(); it != pub_eps.end(); ++it) {
        s_pub.bind(it->c_str());
        syslog(LOG_INFO, "publishing on %s", it->c_str());
    }
    
    // Binding event collection endpoint
    s_events.setsockopt(ZMQ_HWM, &watermark, sizeof(watermark));
    s_events.bind("inproc://events");

    // Initializing regexps
    regcomp(&r_loop, "loop [0-9]+ [0-9]+ [a-z]+://.*", REG_EXTENDED | REG_NOSUB);
    regcomp(&r_unloop, "unloop [a-z]+:[[:alnum:]]{8}", REG_EXTENDED | REG_NOSUB);
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
        { (void*)s_ctl,    0, ZMQ_POLLIN, 0 },
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
            while(s_ctl.recv(&message, ZMQ_NOBLOCK)) {
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
                event_t *event = reinterpret_cast<event_t*>(message.data());
                
                // 5.1. Dropping events from stopped engines
                if(m_engines.find(event->key) != m_engines.end()) {
                    zmq::message_t publication;
                        
                    // 5.2. Disassemble and send in the envelopes
                    for(dict_t::iterator it = event->dict->begin(); it != event->dict->end(); ++it) {
                        std::ostringstream envelope;
                        envelope << event->key << " " << it->first << TIMESTAMP;

                        publication.rebuild(envelope.str().length());
                        memcpy(publication.data(), envelope.str().data(), envelope.str().length());
                        s_pub.send(publication, ZMQ_SNDMORE);

                        publication.rebuild(it->second.length());
                        memcpy(publication.data(), it->second.data(), it->second.length());
                        s_pub.send(publication); 
                    }
                }

                delete event->dict;
            }
        }
    }
}

void core_t::send(const std::string& response) {
    zmq::message_t message(response.length());
    memcpy(message.data(), response.data(), response.length()); 
    
    s_ctl.send(message, ZMQ_NOBLOCK);
}

void core_t::send(const std::vector<std::string>& response) {
    std::vector<std::string>::const_iterator it = response.begin();
    zmq::message_t message;
    
    while(it != response.end()) {
        message.rebuild(it->length());
        memcpy(message.data(), it->data(), it->length());
   
        if(++it == response.end()) {
            s_ctl.send(message, ZMQ_NOBLOCK);
            break;
        } else {
            s_ctl.send(message, ZMQ_SNDMORE);
        }
    }
}

// Message types:
// --------------
// * Loop - launches a thread which fetches data from the
//   specified source and publishes it via the PUB socket. Plugin
//   will be invoked every 'timeout' microsecods, for 'ttl' seconds
//   or forever if ttl = 0:
//   -> loop interval-in-ms ttl-in-s source://parameters
//   <- key|e source|e argument|e runtime
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
//   <- multipart: [field @timestamp, data]|e source|e argument|e runtime
//      all timestamps are equal, message count = field count
//
// * [!] History - fetch historical data without plugin invocation. 
//   You can't fetch more messages than there were invocations:
//   -> history depth source://parameters
//   <- multipart: [field @timestamp, data]|e empty
//      message count = min(depth, history-length)
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
        loop(helpers::uri_t(uri), interval, ttl);
   
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
        once(helpers::uri_t(uri));

        return;
    }

    syslog(LOG_ERR, "got an unsupported request: %s", request.c_str());
    send("e request");
}

void core_t::loop(const helpers::uri_t& uri, time_t interval, time_t ttl) {
    // 2.1.2. Search for the engine
    engines_t::iterator it = m_engines.find(uri.hash);

    if(it != m_engines.end()) {
        // 2.1.3. Increment reference counter and update timers
        it->second->subscribe(interval, ttl);
    } else {
        // 2.1.4. Start a new engine
        try {
            loop_t* engine = new loop_t(uri.hash,
                m_registry.create(uri),
                m_context, interval, ttl);
            m_engines[uri.hash] = engine;
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "failed to instantiate the source: %s", e.what());
            send("e runtime");
            return;
        } catch(const std::invalid_argument& e) {
            syslog(LOG_ERR, "invalid uri: %s", e.what());
            send("e argument");
            return;
        } catch(const std::domain_error& e) {
            syslog(LOG_ERR, "unknown source type: %s", e.what());
            send("e source");
            return;
        }
    }

    // 2.1.5. Return the key
    send(uri.hash);
}

void core_t::unloop(const std::string& key) {
    // 2.2.1. Search for the engine 
    engines_t::iterator it = m_engines.find(key);
    
    if(it == m_engines.end()) {
        syslog(LOG_ERR, "got an invalid key: %s", key.c_str());
        send("e key");
        return;
    }

    // 2.2.2. Decrement reference counter
    it->second->unsubscribe();
    send("ok");
}

void core_t::once(const helpers::uri_t& uri) {
    // 2.3.1. Create a new source
    source_t* source;
        
    try {
        source = m_registry.create(uri);
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "failed to instantiate the source: %s", e.what());
        send("e runtime");
        return;
    } catch(const std::invalid_argument& e) {
        syslog(LOG_ERR, "invalid argument: %s", e.what());
        send("e argument");
        return;
    } catch(const std::domain_error& e) {
        syslog(LOG_ERR, "unknown source type: %s", e.what());
        send("e source");
        return;
    }

    // 2.3.2. Fetch the data once
    dict_t dict = source->fetch();

    if(!dict.size()) {
        send("e empty");
    } else {
        // 2.3.3. Return the formatted response
        std::vector<std::string> response;
        
        for(dict_t::iterator it = dict.begin(); it != dict.end(); ++it) {
            std::ostringstream envelope;
            
            envelope << it->first << TIMESTAMP;
            response.push_back(envelope.str());
            response.push_back(it->second);
        }

        send(response);
    }
    
    delete source;
}
