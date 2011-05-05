#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include <boost/bind.hpp>
#include <boost/tuple/tuple.hpp>

#include <msgpack.hpp>

#include "core.hpp"

using namespace yappi::core;
using namespace yappi::engine;
using namespace yappi::plugin;

const char core_t::identity[] = "yappi";

core_t::core_t(const std::vector<std::string>& listeners,
               const std::vector<std::string>& publishers,
               uint64_t hwm, int64_t swap):
    m_context(1),
    s_sink(m_context, ZMQ_PULL),
    s_listener(m_context, ZMQ_ROUTER),
    s_publisher(m_context, ZMQ_PUB)
{
    // Version dump
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);
    syslog(LOG_INFO, "using libzmq version %d.%d.%d",
        major, minor, patch);
    
    syslog(LOG_INFO, "using libev version %d.%d",
        ev_version_major(), ev_version_minor());

    syslog(LOG_INFO, "using libmsgpack version %s",
        msgpack_version());

    // Initialize sockets
    int fd;
    size_t size = sizeof(fd);

    if(!listeners.size() || !publishers.size()) {
        throw std::runtime_error("at least one listening and one publishing address required");
    }

    // Internal event sink socket
    uint64_t sink_hwm = 1000;
    s_sink.setsockopt(ZMQ_HWM, &sink_hwm, sizeof(sink_hwm));

    s_sink.bind("inproc://sink");

    s_sink.getsockopt(ZMQ_FD, &fd, &size);
    e_sink.set<core_t, &core_t::publish>(this);
    e_sink.start(fd, EV_READ);

    // Listening socket
    for(std::vector<std::string>::const_iterator it = listeners.begin(); it != listeners.end(); ++it) {
        s_listener.bind(it->c_str());
        syslog(LOG_INFO, "listening on %s", it->c_str());
    }

    s_listener.getsockopt(ZMQ_FD, &fd, &size);
    e_listener.set<core_t, &core_t::dispatch>(this);
    e_listener.start(fd, EV_READ);

    // Publishing socket
    s_publisher.setsockopt(ZMQ_HWM, &hwm, sizeof(hwm));
    s_publisher.setsockopt(ZMQ_SWAP, &swap, sizeof(swap));

    for(std::vector<std::string>::const_iterator it = publishers.begin(); it != publishers.end(); ++it) {
        s_publisher.bind(it->c_str());
        syslog(LOG_INFO, "publishing on %s", it->c_str());
    }
    
    // Initialize signal watchers
    e_sigint.set<core_t, &core_t::terminate>(this);
    e_sigint.start(SIGINT);

    e_sigterm.set<core_t, &core_t::terminate>(this);
    e_sigterm.start(SIGTERM);

    e_sigquit.set<core_t, &core_t::terminate>(this);
    e_sigquit.start(SIGQUIT);

    // Initialize built-in command handlers
    m_dispatch["push"] = boost::bind(&core_t::push, this, _1, _2, _3);
    m_dispatch["drop"] = boost::bind(&core_t::drop, this, _1, _2, _3);
    m_dispatch["once"] = boost::bind(&core_t::once, this, _1, _2, _3);
}

core_t::~core_t() {
    syslog(LOG_DEBUG, "shutting down the engines");

    // Stopping the engines
    m_engines.clear();
}

void core_t::run() {
    m_loop.loop();
}

void core_t::terminate(ev::sig& sig, int revents) {
    m_loop.unloop();
}

void core_t::dispatch(ev::io& io, int revents) {
    uint32_t events;
    size_t size = sizeof(events);

    zmq::message_t message;
    identity_t identity;
    
    std::string request;
    Json::Reader reader(Json::Features::strictMode());
    
    Json::Value root, response;
    
    while(true) {
        // Check if we have pending messages
        s_listener.getsockopt(ZMQ_EVENTS, &events, &size);

        if(!(events & ZMQ_POLLIN)) {
            // No more messages, so break the loop
            break;
        }

        // Fetch the client's identity
        while(true) {
            s_listener.recv(&message);

            if(message.size() == 0) {
                // Break if we got a delimiter
                break;
            }

            identity.push_back(std::string(
                reinterpret_cast<const char*>(message.data()),
                message.size()));
        }

        // Fetch the actual request
        s_listener.recv(&message);
        
        request.assign(
            reinterpret_cast<const char*>(message.data()),
            message.size());

        // Try to parse the incoming JSON document
        if(!reader.parse(request, root)) {
            syslog(LOG_ERR, "invalid json: %s", reader.getFormatedErrorMessages().c_str());
            
            response["error"] = reader.getFormatedErrorMessages();
            reply(identity, response);
            
            continue;
        } 

        // Check if root is an object
        if(!root.isObject()) {
            syslog(LOG_ERR, "invalid request: object expected");
            
            response["error"] = "object expected";
            reply(identity, response);
            
            continue;
        }
       
        // Get the action
        Json::Value action = root["action"];

        if(action == Json::Value::null || !action.isString()) {
            syslog(LOG_ERR, "invalid request: invalid action");

            response["error"] = "invalid action";
            reply(identity, response);
            
            continue;
        }

        response["action"] = action;
        
        // Check if the action is supported
        dispatch_map_t::iterator it = m_dispatch.find(action.asString());

        if(it == m_dispatch.end()) {
            syslog(LOG_ERR, "invalid request: action '%s' is not supported", action.asCString());
            
            response["error"] = "action is not supported";
            reply(identity, response);
            
            continue;
        }

        handler_fn_t& handler = it->second;

        // Check if we have any targets for the action
        Json::Value targets = root["targets"];
        
        if(targets == Json::Value::null || !targets.isObject()) {
            syslog(LOG_ERR, "invalid request: no targets specified");

            response["error"] = "no targets specified";
            reply(identity, response);
            
            continue;
        }

        // Iterate over all the targets
        Json::Value::Members names = targets.getMemberNames();

        for(Json::Value::Members::const_iterator name = names.begin(); name != names.end(); ++name) {
            // Get the target args
            Json::Value body = targets[*name];

            // And check if it's an object
            if(!body.isObject()) {
                syslog(LOG_ERR, "invalid request target: object expected");

                response["results"][*name] = Json::Value();
                response["results"][*name]["error"] = "object expected";
                
                continue;
            }

            // Dispatch
            response["results"][*name] = handler(identity, *name, body);
        }

        // Send the response back to the client
        reply(identity, response);
    }
}

void core_t::reply(const identity_t& identity, const Json::Value& root) {
    zmq::message_t message;

    // Send the identity
    for(identity_t::const_iterator it = identity.begin(); it != identity.end(); ++it) {
        message.rebuild(it->length());    
        memcpy(message.data(), it->data(), it->length());
        s_listener.send(message, ZMQ_SNDMORE);
    }

    // Send the delimiter
    message.rebuild(0);
    s_listener.send(message, ZMQ_SNDMORE);

    // Send the json
    Json::FastWriter writer;
    std::string response = writer.write(root);

    message.rebuild(response.length());
    memcpy(message.data(), response.data(), response.length());
    s_listener.send(message);
}

// Built-in commands:
// --------------
// * Push - launches a thread which fetches data from the
//   specified source and publishes it via the PUB socket. Plugin
//   will be invoked every 'timeout' milliseconds
//
// * Drop - shuts down the specified collector.
//   Remaining messages will stay orphaned in the queue,
//   so it's a good idea to drain it after the unsubscription:
//
// * Once - one-time plugin invocation. For one-time invocations,
//   you have no means to filter the data by categories or fields:
//
// * [x] History - fetch historical data without plugin invocation. 
//   You can't fetch more messages than there were invocations:

Json::Value core_t::push(const identity_t& identity, const std::string& target, const Json::Value& args) {
    Json::Value response;
    time_t interval;

    // Parse the arguments
    try {
        interval = args.get("interval", 1000).asUInt();
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "invalid interval: %s", e.what());
        
        response["error"] = std::string("invalid interval: ") + e.what();
        return response;
    }

    // Check if we have an engine running for the given uri
    engine_map_t::iterator it = m_engines.find(target); 

    if(it == m_engines.end()) {
        try {
            // No engine was found, so try to start a new one
            engine_t* engine = new engine_t(m_registry.instantiate(target), m_context);
            boost::tie(it, boost::tuples::ignore) = m_engines.insert(target, engine);
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "runtime error: %s", e.what());
            response["error"] = std::string("runtime error: ") + e.what();
        } catch(const std::invalid_argument& e) {
            syslog(LOG_ERR, "invalid argument: %s", e.what());
            response["error"] = std::string("invalid argument: ") + e.what();
        } catch(const std::domain_error& e) {
            syslog(LOG_ERR, "unknown source: %s", e.what());
            response["error"] = std::string("unknown source: ") + e.what();
        }
    }

    if(!response.isMember("error")) {
        // Schedule
        std::string key = it->second->schedule(identity, interval);
        
        // Store the key into a weak mapping
        m_subscriptions[key] = it->second;
        response["key"] = key;
    }

    return response;
}

Json::Value core_t::drop(const identity_t& identity, const std::string& target, const Json::Value& args) {
    Json::Value response;

    // Check if we have such a subscription
    weak_engine_map_t::iterator it = m_subscriptions.find(target);

    if(it == m_subscriptions.end()) {
        syslog(LOG_ERR, "subscription key not found: %s", target.c_str());
        response["error"] = "not found";
    } else {
        try {
            // Try to unsubscribe the client
            it->second->deschedule(identity, target);
            m_subscriptions.erase(it);
            response["result"] = "success";
        } catch(const std::invalid_argument& e) {
            syslog(LOG_ERR, "invalid argument: %s", e.what());
            response["error"] = std::string("invalid argument: ") + e.what();
        }
    }

    return response;
}

Json::Value core_t::once(const identity_t& identity, const std::string& target, const Json::Value& args) {
    Json::Value response;
    source_t* source = NULL;

    try {
        // Try to instantiate the source
        source = m_registry.instantiate(target);
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "runtime error: %s", e.what());
        response["error"] = std::string("runtime error: ") + e.what();
    } catch(const std::invalid_argument& e) {
        syslog(LOG_ERR, "invalid argument: %s", e.what());
        response["error"] = std::string("invalid argument: ") + e.what();
    } catch(const std::domain_error& e) {
        syslog(LOG_ERR, "unknown source: %s", e.what());
        response["error"] = std::string("unknown source: ") + e.what();
    }

    if(!response.isMember("error") && source) {
        // Fetch the data
        dict_t dict = source->fetch();

        if(dict.size()) {
            for(dict_t::const_iterator it = dict.begin(); it != dict.end(); ++it) {
                response[it->first] = it->second;
            }
        }

        delete source;
    }

    return response;
}

// Publishing format (not JSON, as it will render subscription mechanics pointless):
// ------------------
//   multipart: [key field @timestamp] [value]

void core_t::publish(ev::io& io, int revents) {
    unsigned long events;
    size_t size = sizeof(events);

    zmq::message_t message;
    std::string key;
    dict_t dict;
    
    while(true) {
        // Check if we really have a message
        s_sink.getsockopt(ZMQ_EVENTS, &events, &size);

        if(!(events & ZMQ_POLLIN)) {
            break;
        }

        // Receive the key
        s_sink.recv(&message);
        key.assign(
            reinterpret_cast<const char*>(message.data()),
            message.size());
    
        // Receive the data
        s_sink.recv(&message);
        
        msgpack::unpacked unpacked;
        msgpack::unpack(&unpacked,
            reinterpret_cast<const char*>(message.data()),
            message.size());
        msgpack::object object = unpacked.get();
        object.convert(&dict);

        // Disassemble and send in the envelopes
        for(dict_t::const_iterator it = dict.begin(); it != dict.end(); ++it) {
            std::ostringstream envelope;
            envelope << key << " " << it->first << " @" 
                     << std::fixed << std::setprecision(3) << m_loop.now();

            message.rebuild(envelope.str().length());
            memcpy(message.data(), envelope.str().data(), envelope.str().length());
            s_publisher.send(message, ZMQ_SNDMORE);

            message.rebuild(it->second.length());
            memcpy(message.data(), it->second.data(), it->second.length());
            s_publisher.send(message);
        }

        dict.clear();
    }
}
