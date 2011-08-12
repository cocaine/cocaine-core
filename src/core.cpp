#include <iomanip>
#include <sstream>

#include <boost/bind.hpp>
#include <boost/tuple/tuple.hpp>
#include <msgpack.hpp>

#include "core.hpp"
#include "future.hpp"

using namespace yappi::core;
using namespace yappi::engine;
using namespace yappi::plugin;

core_t::core_t(const std::string& uuid,
               const std::vector<std::string>& listeners,
               const std::vector<std::string>& publishers,
               uint64_t hwm, bool purge):
    m_registry("/usr/lib/yappi") /* [CONFIG] */,
    m_signer(uuid),
    m_storage(uuid),
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

    syslog(LOG_INFO, "core: using libzmq version %d.%d.%d", major, minor, patch);
    syslog(LOG_INFO, "core: using libev version %d.%d", ev_version_major(), ev_version_minor());
    syslog(LOG_INFO, "core: using libmsgpack version %s", msgpack_version());
    syslog(LOG_INFO, "core: instance uuid - %s", uuid.c_str());

    // Internal event sink socket
    s_events.bind("inproc://events");
    e_events.set<core_t, &core_t::event>(this);
    e_events.start(s_events.fd(), EV_READ);

    // Internal future sink socket
    s_futures.bind("inproc://futures");
    e_futures.set<core_t, &core_t::future>(this);
    e_futures.start(s_futures.fd(), EV_READ);

    // Internal engine reaping requests sink
    s_reaper.bind("inproc://reaper");
    e_reaper.set<core_t, &core_t::reap>(this);
    e_reaper.start(s_reaper.fd(), EV_READ);

    // Listening socket
    for(std::vector<std::string>::const_iterator it = listeners.begin(); it != listeners.end(); ++it) {
        s_requests.bind(*it);
        syslog(LOG_INFO, "core: listening for requests on %s", it->c_str());
    }

    e_requests.set<core_t, &core_t::request>(this);
    e_requests.start(s_requests.fd(), EV_READ);

    // Publishing socket
    s_publisher.setsockopt(ZMQ_HWM, &hwm, sizeof(hwm));

    for(std::vector<std::string>::const_iterator it = publishers.begin(); it != publishers.end(); ++it) {
        s_publisher.bind(*it);
        syslog(LOG_INFO, "core: publishing events on %s", it->c_str());
    }

    // Initialize signal watchers
    e_sigint.set<core_t, &core_t::terminate>(this);
    e_sigint.start(SIGINT);

    e_sigterm.set<core_t, &core_t::terminate>(this);
    e_sigterm.start(SIGTERM);

    e_sigquit.set<core_t, &core_t::terminate>(this);
    e_sigquit.start(SIGQUIT);

    e_sighup.set<core_t, &core_t::reload>(this);
    e_sighup.start(SIGHUP);

    // Built-ins
    m_dispatch["push"] = boost::bind(&core_t::push, this, _1, _2, _3);
    m_dispatch["drop"] = boost::bind(&core_t::drop, this, _1, _2, _3);

    if(purge) {
        m_storage.purge();
    }

    // Recover persistent tasks
    recover();
}

core_t::~core_t() {
    syslog(LOG_INFO, "core: shutting down the engines");

    // Clearing up all the pending futures
    m_futures.clear();
    
    // Stopping the engines
    m_engines.clear();
}

void core_t::run() {
    m_loop.loop();
}

void core_t::terminate(ev::sig& sig, int revents) {
    m_loop.unloop();
}

void core_t::reload(ev::sig& sig, int revents) {
    syslog(LOG_NOTICE, "core: reloading tasks");

    m_futures.clear();
    m_engines.clear();

    recover();
}

void core_t::request(ev::io& io, int revents) {
    zmq::message_t message, signature;
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

        // Construct the remote future
        future_t* future = new future_t(this, identity);
        m_futures.insert(future->id(), future);

        // Fetch the request
        s_requests.recv(&message);
        
        request.assign(
            static_cast<const char*>(message.data()),
            message.size());
        
        // Fetch the message signature, if any
        if(s_requests.has_more()) {
            s_requests.recv(&signature);
        } else {
            signature.rebuild();
        }

        // Try to parse the incoming JSON document
        if(!reader.parse(request, root)) {
            syslog(LOG_ERR, "core: invalid json - %s",
                reader.getFormatedErrorMessages().c_str());
            future->fulfill("error", reader.getFormatedErrorMessages());
            continue;
        } 

        // Check if root is an object
        if(!root.isObject()) {
            syslog(LOG_ERR, "core: invalid request - object expected");
            future->fulfill("error", "object expected");
            continue;
        }
       
        // Check the version
        Json::Value version = root["version"];

        if(!version.isIntegral() || version.asInt() < 2) {
            syslog(LOG_ERR, "core: invalid request - invalid protocol version");
            future->fulfill("error", "invalid protocol version");
            continue;
        }

        future->set("protocol", "2");
      
        // Security
        Json::Value token = root["token"];
        
        if(!token.isString()) {
            syslog(LOG_ERR, "core: invalid request - security token expected");
            future->fulfill("error", "security token expected");
            continue;
        } else if(version.asInt() > 2) {
            try {
                m_signer.verify(request, static_cast<const unsigned char*>(signature.data()),
                    signature.size(), token.asString());
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "core: unauthorized access - %s", e.what());
                future->fulfill("error", "unauthorized access");
                continue;
            }
        }

        future->set("token", token.asString());

        // Check if we have any targets for the action
        Json::Value targets = root["targets"];
        
        if(!targets.isObject() || !targets.size()) {
            syslog(LOG_ERR, "core: invalid request - no targets specified");
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
            Json::Value response;

            // And check if it's an object
            if(!args.isObject()) {
                syslog(LOG_ERR, "core: invalid request - target object expected");
                response["error"] = "target object expected";
                future->fulfill(target, response);
                continue;
            }

            // Get the action, and check if it's supported
            std::string action = args.get("action", "push").asString();
            args.removeMember("action");
            
            dispatch_map_t::iterator actor = m_dispatch.find(action);

            if(actor == m_dispatch.end()) {
                syslog(LOG_ERR, "core: invalid request - action '%s' is not supported",
                    action.c_str());
                response["error"] = "action is not supported";
                future->fulfill(target, response);
                continue;
            }

            // Finally, dispatch
            actor->second(future, target, args);
        }
    }
}

void core_t::seal(const std::string& future_id) {
    zmq::message_t message;
    future_map_t::iterator it = m_futures.find(future_id);

    if(it == m_futures.end()) {
        syslog(LOG_ERR, "core: found an orphan - future %s", future_id.c_str());
        return;
    }
        
    future_t* future = it->second;
    std::vector<std::string> identity = future->identity();

    // Send it if it's not an internal future
    if(!identity.empty()) {
        std::string response = future->seal();
        
        syslog(LOG_DEBUG, "core: sending response to '%s' - future %s", 
            future->get("token").c_str(), future->id().c_str());

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
        s_requests.send(message, future->get("protocol") > "2" ? ZMQ_SNDMORE : 0);

        if(future->get("protocol") > "2") {
            // Send the signature
            std::string signature = m_signer.sign(response, "yappi");
            message.rebuild(signature.length());
            memcpy(message.data(), signature.data(), signature.length());
            s_requests.send(message);
        }
    }

    // Release the future
    m_futures.erase(it);
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

void core_t::push(future_t* future, const std::string& target, const Json::Value& args) {
    Json::Value response;
   
    // Check if we have an engine running for the given uri
    engine_map_t::iterator it = m_engines.find(target); 
    engine_t* engine = NULL;

    if(it == m_engines.end()) {
        try {
            // If the engine wasn't found, try to start a new one
            engine = new engine_t(m_context, m_registry, m_storage, target);
            m_engines.insert(target, engine);
        } catch(const std::exception& e) {
            syslog(LOG_ERR, "core: exception in push() - %s", e.what());
            response["error"] = e.what();
            future->fulfill(target, response);
            return;
        } catch(...) {
            syslog(LOG_CRIT, "core: unexpected exception in push()");
            abort();
        }
    } else {
        engine = it->second;
    }

    // Dispatch!
    engine->push(future, args);
}

void core_t::drop(future_t* future, const std::string& target, const Json::Value& args) {
    Json::Value response;

    engine_map_t::iterator it = m_engines.find(target);
    
    if(it == m_engines.end()) {
        syslog(LOG_ERR, "core: engine %s not found", target.c_str());
        response["error"] = "engine not found";
        future->fulfill(target, response);
        return;
    }

    // Dispatch!
    engine_t* engine = it->second;
    engine->drop(future, args);
}

// Publishing format (not JSON, as it will render subscription mechanics pointless):
// ------------------
//   multipart: [key field timestamp] [blob]

void core_t::event(ev::io& io, int revents) {
    zmq::message_t message;
    std::string scheduler_id;
    dict_t dict;
    
    while(s_events.pending()) {
        // Receive the scheduler id
        s_events.recv(&message);
        scheduler_id.assign(
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
            envelope << scheduler_id << " " << it->first << " "
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
            syslog(LOG_ERR, "core: found an orphan - slice for future %s", 
                message["future"].asCString());
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
            syslog(LOG_ERR, "core: found an orphan - engine %s", message["engine"].asCString());
            continue;
        }
        
        syslog(LOG_DEBUG, "core: suicide requested for thread %s in engine %s",
            message["thread"].asCString(), message["engine"].asCString());

        engine_t* engine = it->second;
        engine->reap(message["thread"].asString());
    }
}

void core_t::recover() {
    Json::Value root = m_storage.all();

    if(root.size()) {
        syslog(LOG_NOTICE, "core: loaded %d task(s)", root.size());
        
        future_t* future = new future_t(this, std::vector<std::string>());
        m_futures.insert(future->id(), future);
        future->await(root.size());
                
        Json::Value::Members ids = root.getMemberNames();
        
        for(Json::Value::Members::const_iterator it = ids.begin(); it != ids.end(); ++it) {
            Json::Value object = root[*it];
            future->set("token", object["token"].asString());
            push(future, object["url"].asString(), object["args"]);
        }
    }
}

