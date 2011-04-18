#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include <boost/bind.hpp>

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

    // Damn it
    m_loop.set_io_collect_interval(0.5);

    s_listener.getsockopt(ZMQ_FD, &fd, &size);
    e_listener.set<core_t, &core_t::dispatch>(this);
    e_listener.start(fd, EV_READ | EV_WRITE);

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
    m_dispatch["subscribe"] = boost::bind(&core_t::subscribe, this, _1, _2, _3);
    m_dispatch["unsubscribe"] = boost::bind(&core_t::unsubscribe, this, _1, _2, _3);
    m_dispatch["once"] = boost::bind(&core_t::once, this, _1, _2, _3);
}

core_t::~core_t() {
    syslog(LOG_DEBUG, "shutting down the engines");

    // Stopping the engines
    for(engine_map_t::iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        delete it->second;
    }
}

void core_t::run() {
    m_loop.loop();
}

void core_t::dispatch(ev::io& io, int revents) {
    uint32_t events;
    size_t size = sizeof(events);

    zmq::message_t message;
    std::deque<std::string> identity;
    std::string request;

    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;
   
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
                reinterpret_cast<char*>(message.data()),
                message.size()));
        }

        // Receive the actual request
        s_listener.recv(&message);
        request.assign(
            reinterpret_cast<char*>(message.data()),
            message.size());

        // Try to parse the incoming JSON document
        if(!reader.parse(request, root)) {
            syslog(LOG_ERR, "invalid json: %s", reader.getFormatedErrorMessages().c_str());
            root.clear();
            root["error"] = reader.getFormatedErrorMessages();
            reply(identity, root);
            continue;
        } 

        // Check if root is a mapping
        if(!root.isObject()) {
            syslog(LOG_ERR, "invalid request: mapping expected");
            root.clear();
            root["error"] = "mapping expected";
            reply(identity, root);
            continue;
        }
        
        // Get all the requested methods' names
        const Json::Value::Members methods = root.getMemberNames();

        // Iterate over all the methods
        for(Json::Value::Members::const_iterator name = methods.begin(); name != methods.end(); ++name) {
            // Check if the method is supported
            if(m_dispatch.find(*name) == m_dispatch.end()) {
                syslog(LOG_ERR, "method %s is not supported", name->c_str());
                root[*name].clear();
                root[*name]["error"] = "not supported";
                continue;
            }
           
            // Get the method body
            Json::Value method = root[*name];

            // And check if it's a mapping
            if(!method.isObject()) {
                syslog(LOG_ERR, "invalid method arguments: mapping expected");
                root[*name].clear();
                root[*name]["error"] = "mapping expected";
                continue;
            }

            // Get the requested URIs
            const Json::Value::Members uris = method.getMemberNames();

            // Iterate over all the requested URIs
            for(Json::Value::Members::const_iterator uri = uris.begin(); uri != uris.end(); ++uri) {
                // Get the requested URI's body
                Json::Value args = root[*name][*uri];

                // And check if it's a mapping
                if(!args.isObject()) {
                    syslog(LOG_ERR, "invalid URI arguments: mapping expected");
                    root[*name][*uri].clear();
                    root[*name][*uri]["error"] = "mapping expected";
                    continue;
                }

                // Dispatch the requested URI and replace the arguments with the result
                m_dispatch[*name](identity, *uri, args);
                root[*name][*uri] = args;
            }
        }

        // Send the response back to the client
        reply(identity, root);
    }
}

// Built-in commands:
// --------------
// * Subscribe - launches a thread which fetches data from the
//   specified source and publishes it via the PUB socket. Plugin
//   will be invoked every 'timeout' milliseconds
//
// * Unsubscribe - shuts down the specified collector.
//   Remaining messages will stay orphaned in the queue,
//   so it's a good idea to drain it after the unsubscription:
//
// * Once - one-time plugin invocation. For one-time invocations,
//   you have no means to filter the data by categories or fields:
//
// * [x] History - fetch historical data without plugin invocation. 
//   You can't fetch more messages than there were invocations:

void core_t::subscribe(const std::deque<std::string>& identity, const std::string& uri, Json::Value& args) {
    time_t interval;

    // Validate arguments
    try {
        interval = args.get("interval", 0).asUInt();
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "invalid interval type: %s", e.what());
        args.clear();
        args["error"] = std::string("invalid interval type: ") + e.what();
        return;
    }

    // Clear the node, as we will put the response in it
    args.clear();
    
    // Validate the interval
    if(interval <= 0) {
        syslog(LOG_ERR, "invalid interval specified");
        args["error"] = "invalid interval";
        return;    
    }

    // Check if we have an engine running for the given uri
    engine_map_t::iterator it = m_engines.find(uri); 
    engine_t* engine;

    if(it != m_engines.end()) {
        engine = it->second;
    } else {
        try {
            // No engine was found, so try to start a new one
            engine = new engine_t(uri, m_registry.instantiate(uri), m_context);
            m_engines[uri] = engine;
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "runtime error: %s", e.what());
            args["error"] = std::string("runtime error: ") + e.what();
            return;
        } catch(const std::invalid_argument& e) {
            syslog(LOG_ERR, "invalid argument: %s", e.what());
            args["error"] = std::string("invalid argument: ") + e.what();
            return;
        } catch(const std::domain_error& e) {
            syslog(LOG_ERR, "unknown source: %s", e.what());
            args["error"] = std::string("unknown source: ") + e.what();
            return;
        }
    }

    // Schedule the URI and return the subscription key
    args["result"] = engine->schedule(identity, interval);
}

void core_t::unsubscribe(const std::deque<std::string>& identity, const std::string& uri, Json::Value& args) {
    time_t interval;

    // Validate the arguments
    try {
        interval = args.get("interval", 0).asUInt();
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "invalid interval type: %s", e.what());
        args.clear();
        args["error"] = std::string("invalid interval type: ") + e.what();
        return;
    }

    // Clear the node, as we will put the response in it
    args.clear();
    
    // Validate the interval
    if(interval <= 0) {
        syslog(LOG_ERR, "invalid interval specified");
        args["error"] = "invalid interval";
        return;    
    }

    // Check if we have an engine for that URI
    engine_map_t::iterator it = m_engines.find(uri);

    if(it == m_engines.end()) {
        syslog(LOG_ERR, "no engine was found for uri: %s", uri.c_str());
        args["error"] = "not found";
    } else {
        try {
            // Try to unsubscribe the client
            it->second->deschedule(identity, interval);
            args["result"] = "success";
        } catch(const std::invalid_argument& e) {
            syslog(LOG_ERR, "invalid argument: %s", e.what());
            args["error"] = std::string("invalid argument: ") + e.what();
        }
    }
}

void core_t::once(const std::deque<std::string>& identity, const std::string& uri, Json::Value& args) {
    // No arguments for that type of command
    args.clear();

    // Instantiate the source
    source_t* source;

    try {
        // Try to instantiate the source
        source = m_registry.instantiate(uri);
    } catch(const std::invalid_argument& e) {
        syslog(LOG_ERR, "invalid argument: %s", e.what());
        args["error"] = std::string("invalid argument: ") + e.what();
        return;
    } catch(const std::domain_error& e) {
        syslog(LOG_ERR, "unknown source: %s", e.what());
        args["error"] = std::string("unknown source: ") + e.what();
        return;
    }

    // Fetch the data
    dict_t dict = source->fetch();

    if(dict.size()) {
        for(dict_t::const_iterator it = dict.begin(); it != dict.end(); ++it) {
            args[it->first] = it->second;
        }
    }
}

// Publishing format:
// ------------------
//   multipart: [key field @timestamp] [value]

void core_t::publish(ev::io& io, int revents) {
    unsigned long events;
    size_t size = sizeof(events);

    zmq::message_t message;
    std::string key;
    dict_t* dict = NULL;
    
    while(true) {
        // Check if we really have a message
        s_sink.getsockopt(ZMQ_EVENTS, &events, &size);
        if(!(events & ZMQ_POLLIN)) {
            break;
        }

        // If we do, receive it
        s_sink.recv(&message);
        key.assign(
            reinterpret_cast<char*>(message.data()),
            message.size());
    
        s_sink.recv(&message);
        memcpy(&dict, message.data(), message.size());

        // Disassemble and send in the envelopes
        for(dict_t::const_iterator it = dict->begin(); it != dict->end(); ++it) {
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

        delete dict;
    }
}

void core_t::terminate(ev::sig& sig, int revents) {
    m_loop.unloop();
}

void core_t::reply(const std::deque<std::string>& identity, const Json::Value& root) {
    zmq::message_t message;

    // Send the identity
    for(std::deque<std::string>::const_iterator it = identity.begin(); it != identity.end(); ++it) {
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
