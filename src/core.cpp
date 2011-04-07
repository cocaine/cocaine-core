#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include "core.hpp"

using namespace yappi::core;
using namespace yappi::engine;
using namespace yappi::plugin;

const char core_t::identity[] = "yappi";

core_t::core_t(const std::vector<std::string>& listeners, const std::vector<std::string>& publishers,
               const std::string& plugin_path):
    m_registry(plugin_path),
    m_context(1),
    s_sink(m_context, ZMQ_PULL),
    s_listener(m_context, ZMQ_REP),
    s_publisher(m_context, ZMQ_PUB),
    m_signal(0)
{
    // Version dump
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);
    syslog(LOG_INFO, "using libzmq version %d.%d.%d",
        major, minor, patch);

    syslog(LOG_INFO, "using libev version %d.%d",
        ev_version_major(), ev_version_minor());

    // Binding request endpoints
    if(!listeners.size()) {
        throw std::runtime_error("no listeners specified");
    }
    
    for(std::vector<std::string>::const_iterator it = listeners.begin(); it != listeners.end(); ++it) {
        s_listener.bind(it->c_str());
        syslog(LOG_INFO, "listening on %s", it->c_str());
    }

    // Binding export endpoints
    if(!publishers.size()) {
        throw std::runtime_error("no publishers specified");
    }

    for(std::vector<std::string>::const_iterator it = publishers.begin(); it != publishers.end(); ++it) {
        s_publisher.bind(it->c_str());
        syslog(LOG_INFO, "publishing on %s", it->c_str());
    }
    
    // Binding event collection endpoint
    s_sink.bind("inproc://sink");

    // Initializing regexps
    regcomp(&r_loop, "start [0-9]+ [a-z]+://.*", REG_EXTENDED | REG_NOSUB);
    regcomp(&r_unloop, "stop [a-z0-9]+", REG_EXTENDED | REG_NOSUB);
    regcomp(&r_once, "once [a-z]+://.*", REG_EXTENDED | REG_NOSUB);
}

core_t::~core_t() {
    syslog(LOG_DEBUG, "shutting down the engines");

    for(engine_map_t::iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        delete it->second;
    }

    // Destroying regexps
    regfree(&r_loop);
    regfree(&r_unloop);
    regfree(&r_once);
}

void core_t::run() {
    zmq::message_t message;

    zmq_pollitem_t sockets[] = {
        { (void*)s_listener, 0, ZMQ_POLLIN, 0 },
        { (void*)s_sink,     0, ZMQ_POLLIN, 0 }
    };

    std::string key;
    ev::tstamp timestamp;
    source_t::dict_t* dict = NULL;

    while(true) {
        // Check for a pending signal
        if(m_signal == SIGINT || m_signal == SIGTERM) {
            break;
        }

        // Poll sockets
        try {
            zmq::poll(sockets, 2, -1);
        } catch(const zmq::error_t& e) {
            if(e.num() == EINTR && (m_signal == SIGINT || m_signal == SIGTERM)) {
                // Got termination signal, so stop the loop
                break;
            } else {
                syslog(LOG_ERR, "something bad happened: %s", e.what());
                break;
            }
        }

        // Fetch new requests
        if(sockets[0].revents & ZMQ_POLLIN) {
            while(s_listener.recv(&message, ZMQ_NOBLOCK)) {
                // Dispatch the request
                std::string request(
                    reinterpret_cast<char*>(message.data()),
                    message.size());
                dispatch(request);
            }
        }

        // Fetch new events
        if(sockets[1].revents & ZMQ_POLLIN) {
            while(s_sink.recv(&message, ZMQ_NOBLOCK)) {
                // Get key
                key.assign(
                    reinterpret_cast<char*>(message.data()),
                    message.size()
                );
            
                // Get timestamp
                s_sink.recv(&message);
                memcpy(&timestamp, message.data(), message.size());

                // Get data
                s_sink.recv(&message);
                memcpy(&dict, message.data(), message.size());

                // Disassemble and send in the envelopes
                for(source_t::dict_t::const_iterator it = dict->begin(); it != dict->end(); ++it) {
                    std::ostringstream envelope;
                    envelope << key << " " << it->first
                             << " @" << std::fixed << timestamp;

                    message.rebuild(envelope.str().length());
                    memcpy(message.data(), envelope.str().data(), envelope.str().length());
                    s_publisher.send(message, ZMQ_SNDMORE);
    
                    message.rebuild(it->second.length());
                    memcpy(message.data(), it->second.data(), it->second.length());
                    s_publisher.send(message); 
                }
            }
            
            delete dict;
        }
    }
}

// Send a string
void core_t::send(const std::string& response) {
    zmq::message_t message(response.length());
    memcpy(message.data(), response.data(), response.length()); 
    s_listener.send(message);
}

// Send a muliple strings
void core_t::send(const std::vector<std::string>& response) {
    std::vector<std::string>::const_iterator it = response.begin();
    zmq::message_t message;
    
    while(it != response.end()) {
        message.rebuild(it->length());
        memcpy(message.data(), it->data(), it->length());
   
        if(++it == response.end()) {
            s_listener.send(message);
            break;
        } else {
            s_listener.send(message, ZMQ_SNDMORE);
        }
    }
}

// Message types:
// --------------
// * Start - launches a thread which fetches data from the
//   specified source and publishes it via the PUB socket. Plugin
//   will be invoked every 'timeout' milliseconds
//   -> start interval-in-ms source://parameters
//   <- key|e source|e argument|e runtime
//
// * Stop - shuts down the specified thread.
//   Remaining messages will stay orphaned in the queue,
//   so it's a good idea to drain it after the unsubscription:
//   -> stop key
//   <- ok|e key
//
// * Once - one-time plugin invocation. For one-time invocations,
//   you have no means to filter the data by categories or fields:
//   -> once source://parameters
//   <- multipart: [field1 @timestamp] [value1] ... |e source|e argument|e runtime
//      all timestamps are equal, message count = field count
//
// * [x] History - fetch historical data without plugin invocation. 
//   You can't fetch more messages than there were invocations:
//   -> history depth source://parameters
//   <- multipart: [field @timestamp, data]|e empty
//      message count = min(depth, history-length)
//
// Publishing format:
// ------------------
//   multipart: [key field @timestamp] [value]

void core_t::dispatch(const std::string& request) {
    std::string cmd;
    std::istringstream fmt(request);

    // NOTE: This is unused for now, but might come in handy
    // If I decide to drop regex and use something else
    // for command validation
    fmt >> std::skipws >> cmd;

    if(regexec(&r_loop, request.c_str(), 0, 0, 0) == 0) {
        std::string uri;
        time_t interval;
        
        fmt >> interval >> uri;
        loop(helpers::uri_t(uri), interval);
   
        return;
    }

    if(regexec(&r_unloop, request.c_str(), 0, 0, 0) == 0) {
        std::string key;
        
        fmt >> key;
        unloop(key);
        
        return;
    }

    if(regexec(&r_once, request.c_str(), 0, 0, 0) == 0) {
        std::string uri;

        fmt >> uri;
        once(helpers::uri_t(uri));

        return;
    }

    syslog(LOG_ERR, "got an unsupported request: %s", request.c_str());
    send("e request");
}

void core_t::loop(const helpers::uri_t& uri, time_t interval) {
    engine_t* engine;
    
    // Check if we have an engine for the specified source
    engine_map_t::iterator it = m_engines.find(uri.hash);

    if(it == m_engines.end()) {
        // No engine, launch one
        try {
            engine = new engine_t(uri.hash, *m_registry.create(uri), m_context);
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
    } else {
        engine = it->second;
    }

    // Get the subscription key and store it into
    // active task list
    std::string key = engine->subscribe(interval);
    m_active[key] = engine;

    send(key);
}

void core_t::unloop(const std::string& key) {
    // Search for the engine
    engine_map_t::iterator it = m_active.find(key);
    
    if(it == m_active.end()) {
        syslog(LOG_ERR, "got an invalid key: %s", key.c_str());
        send("e key");
        return;
    }

    // Unsubscribe the client (which stops the slave in the engine)
    // and remove the key from the active task list
    it->second->unsubscribe(key);
    m_active.erase(it);

    send("ok");
}

void core_t::once(const helpers::uri_t& uri) {
    // Create a new source
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

    // Fetch the data once
    source_t::dict_t dict = source->fetch();

    if(!dict.size()) {
        send("e empty");
    } else {
        // Return the formatted response
        std::vector<std::string> response;
        
        for(source_t::dict_t::iterator it = dict.begin(); it != dict.end(); ++it) {
            std::ostringstream envelope;
            
            envelope << it->first;
            response.push_back(envelope.str());
            response.push_back(it->second);
        }

        send(response);
    }
    
    delete source;
}
