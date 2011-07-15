#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include <boost/bind.hpp>
#include <boost/tuple/tuple.hpp>
#include <msgpack.hpp>

#include "core.hpp"
#include "future.hpp"
#include "id.hpp"

using namespace yappi::core;
using namespace yappi::engine;
using namespace yappi::plugin;

const char core_t::identity[] = "yappi";

core_t::core_t(const std::vector<std::string>& listeners,
               const std::vector<std::string>& publishers,
               uint64_t hwm, int64_t swap):
    m_context(1),
    s_events(m_context, ZMQ_PULL),
    s_requests(m_context, ZMQ_ROUTER),
    s_publisher(m_context, ZMQ_PUB),
    s_futures(m_context, ZMQ_PULL),
    s_reaper(m_context, ZMQ_PULL)
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
    s_events.bind("inproc://events");
    s_events.getsockopt(ZMQ_FD, &fd, &size);
    e_events.set<core_t, &core_t::event>(this);
    e_events.start(fd, EV_READ);

    // Internal future sink socket
    s_futures.bind("inproc://futures");
    s_futures.getsockopt(ZMQ_FD, &fd, &size);
    e_futures.set<core_t, &core_t::future>(this);
    e_futures.start(fd, EV_READ);

    // Internal engine reaping requests sink
    s_reaper.bind("inproc://reaper");
    s_reaper.getsockopt(ZMQ_FD, &fd, &size);
    e_reaper.set<core_t, &core_t::reap>(this);
    e_reaper.start(fd, EV_READ);

    // Listening socket
    for(std::vector<std::string>::const_iterator it = listeners.begin(); it != listeners.end(); ++it) {
        s_requests.bind(it->c_str());
        syslog(LOG_INFO, "listening for requests on %s", it->c_str());
    }

    s_requests.getsockopt(ZMQ_FD, &fd, &size);
    e_requests.set<core_t, &core_t::request>(this);
    e_requests.start(fd, EV_READ);

    // Publishing socket
    s_publisher.setsockopt(ZMQ_HWM, &hwm, sizeof(hwm));
    s_publisher.setsockopt(ZMQ_SWAP, &swap, sizeof(swap));

    for(std::vector<std::string>::const_iterator it = publishers.begin(); it != publishers.end(); ++it) {
        s_publisher.bind(it->c_str());
        syslog(LOG_INFO, "publishing events on %s", it->c_str());
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

    // Try to recover persistent tasks
    recover();
}

core_t::~core_t() {
    syslog(LOG_DEBUG, "shutting down the engines");

    // Stopping the engines
    m_engines.clear();

    // Clearing up all the pending futures
    m_futures.clear();
}

void core_t::run() {
    m_loop.loop();
}

void core_t::terminate(ev::sig& sig, int revents) {
    m_loop.unloop();
}

void core_t::recover() {
    Json::Value root = persistance::file_storage_t("/var/spool/yappi").all();

    if(root.size()) {
        syslog(LOG_DEBUG, "recovered %d task(s)", root.size());
        
        future_t* future = new future_t(this);
        m_futures.insert(future->id(), future);
        future->await(root.size());
                
        Json::Value::Members ids = root.getMemberNames();
        
        for(Json::Value::Members::const_iterator it = ids.begin(); it != ids.end(); ++it) {
            Json::Value object = root[*it];
            future->assign(object["token"].asString());
            push(future, object["url"].asString(), object);
        }
    }
}

void core_t::request(ev::io& io, int revents) {
    zmq::message_t message;
    std::vector<std::string> identity;
    
    std::string request;
    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;
    
    while(s_requests.pending()) {
        // Fetch the client's identity
        while(true) {
            s_requests.recv(&message);

            if(message.size() == 0) {
                // Break if we got a delimiter
                break;
            }

            identity.push_back(std::string(
                static_cast<const char*>(message.data()),
                message.size()));
        }

        // Construct the future!
        future_t* future = new future_t(this, identity);
        m_futures.insert(future->id(), future);

        // Fetch the actual request
        s_requests.recv(&message);
        
        request.assign(
            static_cast<const char*>(message.data()),
            message.size());

        // Try to parse the incoming JSON document
        if(!reader.parse(request, root)) {
            syslog(LOG_ERR, "invalid request: %s", reader.getFormatedErrorMessages().c_str());
            future->fulfill("error", reader.getFormatedErrorMessages());
            continue;
        } 

        // Check if root is an object
        if(!root.isObject()) {
            syslog(LOG_ERR, "invalid request: object expected");
            future->fulfill("error", "object expected");
            continue;
        }
       
        // Check the version
        Json::Value version = root["version"];

        if(version == Json::Value::null || !version.isIntegral()) {
            syslog(LOG_ERR, "invalid request: protocol version expected");
            future->fulfill("error", "version expected");
            continue;
        }

        if(version.asInt() != 2) {
            syslog(LOG_ERR, "invalid request: invalid protocol version");
            future->fulfill("error", "invalid protocol version");
            continue;
        }
      
        // Get the security token 
        Json::Value token = root["token"];
        
        if(token == Json::Value::null || !token.isString()) {
            syslog(LOG_ERR, "invalid request: security token expected");
            future->fulfill("error", "security token expected");
            continue;
        } else {
            future->assign(helpers::digest_t().get(token.asString()));
        }

        // Get the action
        Json::Value action = root["action"];

        if(action == Json::Value::null || !action.isString()) {
            syslog(LOG_ERR, "invalid request: action expected");
            future->fulfill("error", "action expected");
            continue;
        }

        // Check if the action is supported
        dispatch_map_t::iterator it = m_dispatch.find(action.asString());

        if(it == m_dispatch.end()) {
            syslog(LOG_ERR, "invalid request: action '%s' is not supported", action.asCString());
            future->fulfill("error", "action is not supported");
            continue;
        }

        handler_fn_t& actor = it->second;

        // Check if we have any targets for the action
        Json::Value targets = root["targets"];
        
        if(targets == Json::Value::null || !targets.isObject() || !targets.size()) {
            syslog(LOG_ERR, "invalid request: no targets specified");
            future->fulfill("error", "no targets specified");
            continue;
        }

        // Iterate over all the targets
        Json::Value::Members names = targets.getMemberNames();
        future->await(names.size());

        for(Json::Value::Members::const_iterator it = names.begin(); it != names.end(); ++it) {
            std::string target = *it;

            // Get the target args
            Json::Value args = targets[target];

            // And check if it's an object
            if(!args.isObject()) {
                syslog(LOG_ERR, "invalid request: target object expected");
                
                Json::Value value;
                value["error"] = "target object expected";
                
                future->fulfill(target, value);
                continue;
            }

            // Finally, dispatch
            actor(future, target, args);
        }
    }
}

void core_t::seal(const std::string& future_id) {
    zmq::message_t message;
    future_map_t::iterator it = m_futures.find(future_id);

    if(it == m_futures.end()) {
        syslog(LOG_ERR, "found an orphaned future, id: %s", future_id.c_str());
    } else {
        future_t* future = it->second;

        std::string response = future->seal();
        std::vector<std::string> identity = future->identity();

        if(identity.empty()) {
            // This is an internal future, simply drop it
            m_futures.erase(it);
            return;
        }

        syslog(LOG_DEBUG, "sending response, future id %s", future->id().c_str());

        // Send the identity
        for(std::vector<std::string>::const_iterator id = identity.begin(); id != identity.end(); ++id) {
            message.rebuild(id->length());
            memcpy(message.data(), id->data(), id->length());
            s_requests.send(message, ZMQ_SNDMORE);
        }
        
        // Send the delimiter
        message.rebuild(0);
        s_requests.send(message, ZMQ_SNDMORE);

        // Send the JSON
        message.rebuild(response.length());
        memcpy(message.data(), response.data(), response.length());
        s_requests.send(message);

        // Release the future
        m_futures.erase(it);
    }
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

void core_t::push(future_t* future, const std::string& target, const Json::Value& args) {
    Json::Value response;
    
    // Parse the arguments
    time_t interval;
    bool transient;

    try {
        interval = args.get("interval", 60000).asUInt();
        transient = args.get("transient", false).asBool();
    } catch(const std::runtime_error& e) {
        response["error"] = e.what();
        future->fulfill(target, response);
        return;
    }

    // Check if we have an engine running for the given uri
    engine_map_t::iterator it = m_engines.find(target); 
    source_t* source = NULL;
    engine_t* engine = NULL;

    if(it == m_engines.end()) {
        try {
            // If the engine wasn't found, try to start a new one
            source = m_registry.instantiate(target);
            engine = new engine_t(m_context, source);
            m_engines.insert(target, engine);
        } catch(const std::exception& e) {
            syslog(LOG_ERR, "exception in core_t::push() - %s", e.what());
            response["error"] = e.what();
            future->fulfill(target, response);
            return;
        } catch(...) {
            syslog(LOG_ERR, "unexpected exception in core_t::push()");
            abort();
        }
    } else {
        engine = it->second;
    }

    // Schedule
    engine->push(future, interval);
}

void core_t::drop(future_t* future, const std::string& target, const Json::Value& args) {
    Json::Value response;
    engine_map_t::iterator it = m_engines.find(target);

    // Parse the arguments
    time_t interval;

    try {
        interval = args.get("interval", 60000).asUInt();
    } catch(const std::runtime_error& e) {
        response["error"] = e.what();
        future->fulfill(target, response);
        return;
    }
    
    if(it == m_engines.end()) {
        syslog(LOG_ERR, "invalid engine url specified: %s", target.c_str());
        response["error"] = "engine not found";
        future->fulfill(target, response);
        return;
    }

    engine_t* engine = it->second;
    engine->drop(future, interval);
}

void core_t::once(future_t* future, const std::string& target, const Json::Value& args) {
    Json::Value response;

    // Check if we have an engine running for the given uri
    engine_map_t::iterator it = m_engines.find(target); 
    source_t* source = NULL;
    engine_t* engine = NULL;

    if(it == m_engines.end()) {
        try {
            // If the engine wasn't found, try to start a new one
            source = m_registry.instantiate(target);
            engine = new engine_t(m_context, source);
            m_engines.insert(target, engine);
        } catch(const std::exception& e) {
            syslog(LOG_ERR, "exception in core_t::once() - %s", e.what());
            response["error"] = e.what();
            future->fulfill(target, response);
            return;
        } catch(...) {
            syslog(LOG_ERR, "unexpected exception in core_t::once()");
            abort();
        }
    } else {
        engine = it->second;
    }

    engine->once(future);
}

// Publishing format (not JSON, as it will render subscription mechanics pointless):
// ------------------
//   multipart: [key field timestamp] [blob]

void core_t::event(ev::io& io, int revents) {
    zmq::message_t message;
    std::string key;
    dict_t dict;
    
    while(s_events.pending()) {
        // Receive the key
        s_events.recv(&message);
        key.assign(
            static_cast<const char*>(message.data()),
            message.size());
    
        // Receive the data
        s_events.recv(&message);
        
        msgpack::unpacked unpacked;
        msgpack::unpack(&unpacked,
            static_cast<const char*>(message.data()),
            message.size());
        msgpack::object object = unpacked.get();
        object.convert(&dict);

        // Disassemble and send in the envelopes
        for(dict_t::const_iterator it = dict.begin(); it != dict.end(); ++it) {
            std::ostringstream envelope;
            envelope << key << " " << it->first << " "
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

void core_t::future(ev::io& io, int revents) {
    while(s_futures.pending()) {
        Json::Value message;
        s_futures.recv(message);

        future_map_t::iterator it = m_futures.find(message["future"].asString());
        
        if(it == m_futures.end()) {
            syslog(LOG_ERR, "orphaned future slice received: %s", message["future"].asCString());
            continue;
        }

        future_t* future = it->second;
        future->fulfill(message["engine"].asString(), message["result"]);
    }
}

void core_t::reap(ev::io& io, int revents) {
    while(s_reaper.pending()) {
        Json::Value message;
        s_reaper.recv(message);

        engine_map_t::iterator it = m_engines.find(message["engine"].asString());

        if(it == m_engines.end()) {
            syslog(LOG_ERR, "orphaned stalled engine wants to be reaped: %s",
                message["engine"].asCString());
            continue;
        }
        
        syslog(LOG_DEBUG, "reaping stalled engine %s",
            message["engine"].asCString());

        m_engines.erase(it);
    }
}
