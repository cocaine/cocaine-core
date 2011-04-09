#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include "core.hpp"

using namespace yappi::core;
using namespace yappi::engine;
using namespace yappi::plugin;

const char core_t::identity[] = "yappi";

core_t::core_t(const std::vector<std::string>& listeners,
               const std::vector<std::string>& publishers):
    m_context(1),
    s_sink(m_context, ZMQ_PULL),
    s_listener(m_context, ZMQ_REP),
    s_publisher(m_context, ZMQ_PUB)
{
    // Version dump
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);
    syslog(LOG_INFO, "using libzmq version %d.%d.%d",
        major, minor, patch);

    syslog(LOG_INFO, "using libev version %d.%d",
        ev_version_major(), ev_version_minor());

    // Initializing sockets
    int fd;
    size_t size = sizeof(fd);

    if(!listeners.size() || !publishers.size()) {
        throw std::runtime_error("at least one listening and one publishing endpoint required");
    }
    
    // Internal event sink socket
    s_sink.bind("inproc://sink");
    s_sink.getsockopt(ZMQ_FD, &fd, &size);
    e_sink.set<core_t, &core_t::publish>(this);
    e_sink.start(fd, EV_READ | EV_WRITE);

    // Listening socket
    for(std::vector<std::string>::const_iterator it = listeners.begin(); it != listeners.end(); ++it) {
        s_listener.bind(it->c_str());
        syslog(LOG_INFO, "listening on %s", it->c_str());
    }

    s_listener.getsockopt(ZMQ_FD, &fd, &size);
    e_listener.set<core_t, &core_t::dispatch>(this);
    e_listener.start(fd, EV_READ | EV_WRITE);

    // Publishing socket
    for(std::vector<std::string>::const_iterator it = publishers.begin(); it != publishers.end(); ++it) {
        s_publisher.bind(it->c_str());
        syslog(LOG_INFO, "publishing on %s", it->c_str());
    }
    
    // Initializing regexps
    regcomp(&r_start, "start [0-9]+ [a-z]+://.*", REG_EXTENDED | REG_NOSUB);
    regcomp(&r_stop, "stop [a-z0-9]+", REG_EXTENDED | REG_NOSUB);
    regcomp(&r_once, "once [a-z]+://.*", REG_EXTENDED | REG_NOSUB);

    // Initializing signal watchers
    e_sigint.set<core_t, &core_t::terminate>(this);
    e_sigint.start(SIGINT);

    e_sigterm.set<core_t, &core_t::terminate>(this);
    e_sigterm.start(SIGTERM);

    e_sigquit.set<core_t, &core_t::terminate>(this);
    e_sigquit.start(SIGQUIT);
}

core_t::~core_t() {
    syslog(LOG_DEBUG, "shutting down the engines");

    // Stopping the engines
    for(engine_map_t::iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        delete it->second;
    }

    // Destroying regexps
    regfree(&r_start);
    regfree(&r_stop);
    regfree(&r_once);
}

void core_t::run() {
    m_loop.loop();
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
//
// * [x] History - fetch historical data without plugin invocation. 
//   You can't fetch more messages than there were invocations:
//   -> history depth source://parameters
//   <- multipart: [field @timestamp, data]|e empty
//      message count = min(depth, history-length)

void core_t::dispatch(ev::io& io, int revents) {
    // Check if we really have a message
    unsigned long events;
    size_t size = sizeof(events);

    s_listener.getsockopt(ZMQ_EVENTS, &events, &size);

    if(!(events & ZMQ_POLLIN)) {
        return;
    }

    // If we do, receive it
    zmq::message_t message;
    s_listener.recv(&message);

    std::string request(
        reinterpret_cast<char*>(message.data()),
        message.size()
    );

    // Try to match the request against the templates
    std::string cmd;
    std::istringstream fmt(request);

    // NOTE: This is unused for now, but might come in handy
    // If I decide to drop regex and use something else
    // for command validation
    fmt >> std::skipws >> cmd;

    if(regexec(&r_start, request.c_str(), 0, 0, 0) == 0) {
        std::string uri;
        time_t interval;
        
        fmt >> interval >> uri;
        start(uri, interval);
   
        return;
    }

    if(regexec(&r_stop, request.c_str(), 0, 0, 0) == 0) {
        std::string key;
        
        fmt >> key;
        stop(key);
        
        return;
    }

    if(regexec(&r_once, request.c_str(), 0, 0, 0) == 0) {
        std::string uri;

        fmt >> uri;
        once(uri);

        return;
    }

    syslog(LOG_ERR, "got an unsupported request: %s", request.c_str());
    send("e request");
}

void core_t::start(const std::string& uri, time_t interval) {
    engine_t* engine;
    
    // Check if we have an engine for the given uri
    engine_map_t::iterator it = m_engines.find(uri); 

    if(it != m_engines.end()) {
        engine = it->second;
    } else {
        // There's no engine for the given identity, so start one
        try {
            engine = new engine_t(uri, *m_registry.instantiate(uri), m_context);
            m_engines[uri] = engine;
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

    // Get the subscription key and store it into
    // active task list
    std::string key = engine->subscribe(interval);
    m_subscriptions[key] = engine;

    send(key);
}

void core_t::stop(const std::string& key) {
    // Search for the engine
    engine_map_t::iterator it = m_subscriptions.find(key);
    
    if(it == m_subscriptions.end()) {
        syslog(LOG_ERR, "got an invalid key: %s", key.c_str());
        send("e key");
        return;
    }

    // Unsubscribe the client (which stops the slave in the engine)
    // and remove the key from the active task list
    it->second->unsubscribe(key);
    m_subscriptions.erase(it);

    send("ok");
}

void core_t::once(const std::string& uri) {
    // Create a new source
    source_t* source;
        
    try {
        source = m_registry.instantiate(uri);
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
    dict_t dict = source->fetch();

    if(!dict.size()) {
        send("e empty");
    } else {
        // Return the formatted response
        std::vector<std::string> response;
        
        for(dict_t::iterator it = dict.begin(); it != dict.end(); ++it) {
            std::ostringstream envelope;
            
            envelope << it->first;
            response.push_back(envelope.str());
            response.push_back(it->second);
        }

        send(response);
    }
    
    delete source;
}

// Publishing format:
// ------------------
//   multipart: [key field @timestamp] [value]

void core_t::publish(ev::io& io, int revents) {
    // Check if we really have a message
    unsigned long events;
    size_t size = sizeof(events);

    s_sink.getsockopt(ZMQ_EVENTS, &events, &size);

    if(!(events & ZMQ_POLLIN)) {
        return;
    }

    // If we do, receive it
    zmq::message_t message;
    
    // Subscription key
    s_sink.recv(&message);
    std::string key(
        reinterpret_cast<char*>(message.data()),
        message.size());
    
    // Event data
    s_sink.recv(&message);
    dict_t* dict = NULL;
    memcpy(&dict, message.data(), message.size());

    // Disassemble and send in the envelopes
    for(dict_t::const_iterator it = dict->begin(); it != dict->end(); ++it) {
        std::ostringstream envelope;
        envelope << key << " " << it->first << " @" 
                 << std::fixed << m_loop.now();

        message.rebuild(envelope.str().length());
        memcpy(message.data(), envelope.str().data(), envelope.str().length());
        s_publisher.send(message, ZMQ_SNDMORE);

        message.rebuild(it->second.length());
        memcpy(message.data(), it->second.data(), it->second.length());
        s_publisher.send(message);
    }

    delete dict;   
}

void core_t::terminate(ev::sig& sig, int revents) {
    m_loop.unloop();
}

void core_t::send(const std::string& response) {
    zmq::message_t message(response.length());
    memcpy(message.data(), response.data(), response.length()); 
    s_listener.send(message);
}

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
