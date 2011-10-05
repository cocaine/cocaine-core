#include <iomanip>
#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/assign.hpp>

#include "cocaine/core.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/future.hpp"
#include "cocaine/storage.hpp"
#include "cocaine/threading.hpp"

using namespace cocaine::core;
using namespace cocaine::engine;
using namespace cocaine::plugin;

core_t::core_t():
    m_context(1),
    s_requests(m_context, ZMQ_ROUTER),
    s_publisher(m_context, ZMQ_PUB),
    s_upstream(m_context, ZMQ_PULL)
{
    // Version dump
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);

    syslog(LOG_INFO, "core: using libzmq version %d.%d.%d", major, minor, patch);
    syslog(LOG_INFO, "core: using libev version %d.%d", ev_version_major(), ev_version_minor());
    syslog(LOG_INFO, "core: using libmsgpack version %s", msgpack_version());

    // Fetching the hostname
    char hostname[256];

    if(gethostname(hostname, 256) == 0) {
        m_hostname = hostname;
    } else {
        throw std::runtime_error("failed to determine the hostname");
    }

    // Interthread channel
    s_upstream.bind("inproc://core");
    e_upstream.set<core_t, &core_t::upstream>(this);
    e_upstream.start(s_upstream.fd(), EV_READ);

    // Listening socket
    for(std::vector<std::string>::const_iterator it = config_t::get().net.listen.begin(); it != config_t::get().net.listen.end(); ++it) {
        s_requests.bind(*it);
        syslog(LOG_INFO, "core: listening for requests on %s", it->c_str());
    }

    e_requests.set<core_t, &core_t::request>(this);
    e_requests.start(s_requests.fd(), EV_READ);

    // Publishing socket
#if ZMQ_VERSION > 30000
    s_publisher.setsockopt(ZMQ_SNDHWM, &config_t::get().net.watermark,
        sizeof(config_t::get().net.watermark));
#else
    s_publisher.setsockopt(ZMQ_HWM, &config_t::get().net.watermark,
        sizeof(config_t::get().net.watermark));
#endif

    for(std::vector<std::string>::const_iterator it = config_t::get().net.publish.begin(); it != config_t::get().net.publish.end(); ++it) {
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

    e_sigusr1.set<core_t, &core_t::purge>(this);
    e_sigusr1.start(SIGUSR1);

    // Task recovery
    recover();
}

core_t::~core_t() {
    syslog(LOG_INFO, "core: shutting down the engines");
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

    m_engines.clear();
    m_futures.clear();
    m_histories.clear();

    try {
        recover();
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "core: [%s()] storage failure - %s", __func__, e.what());
    }
}

void core_t::purge(ev::sig& sig, int revents) {
    syslog(LOG_NOTICE, "core: purging the tasks");
    
    m_engines.clear();
    m_futures.clear();
    m_histories.clear();

    try {
        storage::storage_t::instance()->purge("tasks");
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "core: [%s()] storage failure - %s", __func__, e.what());
    }    
}

void core_t::request(ev::io& io, int revents) {
    zmq::message_t message, signature;
    std::vector<std::string> route;
    std::string request;
    
    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;

    while(s_requests.pending()) {
        route.clear();
        
        while(true) {
            s_requests.recv(&message);

            if(message.size() == 0) {
                // Break if we got a delimiter
                break;
            }

            route.push_back(std::string(
                static_cast<const char*>(message.data()),
                message.size()));
        }

        // Receive the request
        s_requests.recv(&message);

        request.assign(static_cast<const char*>(message.data()),
            message.size());

        // Receive the signature, if it's there
        signature.rebuild();

        if(s_requests.has_more()) {
            s_requests.recv(&signature);
        }

        // Construct the future
        future_t* future = new future_t(this, route);
        m_futures.insert(future->id(), future);
       
        // Parse the request
        root.clear();

        if(reader.parse(request, root)) {
            try {
                if(!root.isObject()) {
                    throw std::runtime_error("root object expected");
                }

                unsigned int version = root.get("version", 1).asUInt();
                std::string username(root.get("username", "").asString());
                
                if(version < 2) {
                    throw std::runtime_error("outdated protocol version");
                }
      
                if(!username.empty()) {
                    if(version > 2) {
                        m_signatures.verify(request,
                            static_cast<const unsigned char*>(signature.data()),
                            signature.size(), username);
                    }
                } else {
                    throw std::runtime_error("username expected");
                }

                // Request dispatching is performed in this function
                dispatch(future, root); 
            } catch(const std::exception& e) {
                syslog(LOG_ERR, "core: [%s()] %s", __func__, e.what());
                future->abort(e.what());
            }
        } else {
            syslog(LOG_ERR, "core: [%s()] %s", __func__,
                reader.getFormatedErrorMessages().c_str());
            future->abort(reader.getFormatedErrorMessages());
        }
    }
}

void core_t::dispatch(future_t* future, const Json::Value& root) {
    std::string action(root.get("action", "push").asString());

    if(action == "push" || action == "drop" || action == "past") {
        Json::Value targets(root["targets"]);

        if(!targets.isObject() || !targets.size()) {
            throw std::runtime_error("no targets specified");
        }

        // Iterate over all the targets
        Json::Value::Members names(targets.getMemberNames());
        future->reserve(names);

        for(Json::Value::Members::iterator it = names.begin(); it != names.end(); ++it) {
            // Get the target and args
            std::string target(*it);
            Json::Value args(targets[target]);

            // Invoke the handler
            try {
                if(args.isObject()) {
                    if(action == "push") {
                        push(future, target, args);
                    } else if(action == "drop") {
                        drop(future, target, args);
                    } else if(action == "past") {
                        past(future, target, args);
                    }
                } else {
                    throw std::runtime_error("arguments expected");
                }
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "core: [%s()] %s", __func__, e.what());
                future->abort(target, e.what());
            }
        }
    } else if(action == "stats") {
        stat(future);
    } else {
        throw std::runtime_error("unsupported action");
    }
}

// Built-in commands:
// ------------------
// * Push - launches a thread which fetches data from the
//   specified source and publishes it via the PUB socket.
//
// * Drop - shuts down the specified collector.
//   Remaining messages will stay orphaned in the queue,
//   so it's a good idea to drain it after the unsubscription:
//
// * Past - fetches the event history for the specified subscription key
//
// * Stat - fetches the current running stats

void core_t::push(future_t* future, const std::string& target, const Json::Value& args) {
    // Check if we have an engine running for the given uri
    std::string uri(args["uri"].asString());
    
    if(uri.empty()) {
        // Backward compatibility
        // throw std::runtime_error("no source uri has been specified");
        uri = target;
    }
    
    engine_map_t::iterator it(m_engines.find(uri)); 

    if(it == m_engines.end()) {
        // If the engine wasn't found, try to start a new one
        boost::tie(it, boost::tuples::ignore) = m_engines.insert(
            uri,
            new engine_t(m_context, uri));
    }

    // Dispatch!
    it->second->push(future, target, args);
}

void core_t::drop(future_t* future, const std::string& target, const Json::Value& args) {
    std::string uri(args["uri"].asString());
        
    if(uri.empty()) {
        // Backward compatibility
        uri = target;
    }

    engine_map_t::iterator it(m_engines.find(uri));
    
    if(it == m_engines.end()) {
        throw std::runtime_error("engine is not active");
    }

    // Dispatch!
    it->second->drop(future, target, args);
}

void core_t::past(future_t* future, const std::string& target, const Json::Value& args) {
    std::string key(args["key"].asString());

    if(key.empty()) {
        // Backward compatibility
        // throw std::runtime_error("no driver id has been specified");
        key = target;
    }

    history_map_t::iterator it(m_histories.find(key));

    if(it == m_histories.end()) {
        throw std::runtime_error("the past for a given key is empty");
    }

    Json::Value result(Json::arrayValue);
    uint32_t depth = args.get("depth", config_t::get().core.history_depth).asUInt(),
             counter = 0;

    for(history_t::const_iterator event = it->second->begin(); event != it->second->end(); ++event) {
        Json::Value object(Json::objectValue);

        object["timestamp"] = event->first;
        object["event"] = event->second;

        result.append(object);

        if(++counter == depth) {
            break;
        }
    }

    future->push(target, result);
}

void core_t::stat(future_t* future) {
    Json::Value engines(Json::objectValue),
                threads(Json::objectValue),
                requests(Json::objectValue);

    future->reserve(boost::assign::list_of
        ("engines")
        ("threads")
        ("requests")
    );

    engines["total"] = engine::engine_t::objects_created;
    
    for(engine_map_t::const_iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        engines["alive"].append(it->first);
    }
    
    future->push("engines", engines);
    
    threads["total"] = engine::threading::thread_t::objects_created;
    threads["alive"] = engine::threading::thread_t::objects_alive;
    future->push("threads", threads);

    requests["total"] = future_t::objects_created;
    requests["pending"] = future_t::objects_alive;
    future->push("requests", requests);
}

void core_t::upstream(ev::io& io, int revents) {
    unsigned int code = 0;
   
    while(s_upstream.pending()) {
        s_upstream.recv(code);

        switch(code) {
            case EVENT: {
                std::string thread_id, driver_id;
                Json::Value object;

                boost::tuple<std::string&, std::string&, Json::Value&>
                    tier(thread_id, driver_id, object);

                s_upstream.recv_multi(tier);
                event(driver_id, object);

                break;
            }
            case FUTURE: {
                std::string future_id, key;
                Json::Value object;
                
                boost::tuple<std::string&, std::string&, Json::Value&>
                    tier(future_id, key, object);

                s_upstream.recv_multi(tier);
                future(future_id, key, object);
                
                break;
            }
            case SUICIDE: {
                std::string engine_id, thread_id;
                
                boost::tuple<std::string&, std::string&> tier(engine_id, thread_id);
                
                s_upstream.recv_multi(tier);
                reap(engine_id, thread_id);
                
                break;
            }
            case TRACK:
            default:
                syslog(LOG_ERR, "core: [%s()] unknown message", __func__);
        }
    }
}

// Publishing format (not JSON, as it will render subscription mechanics pointless):
// ------------------
//   multipart: [key field hostname timestamp] [blob]

void core_t::event(const std::string& driver_id, const Json::Value& event) {
    zmq::message_t message;
    ev::tstamp now = m_loop.now();

    // Maintain the history for the given driver
    history_map_t::iterator history(m_histories.find(driver_id));

    if(history == m_histories.end()) {
        boost::tie(history, boost::tuples::ignore) = m_histories.insert(driver_id,
            new history_t());
    } else if(history->second->size() == config_t::get().core.history_depth) {
        history->second->pop_back();
    }
    
    history->second->push_front(std::make_pair(now, event));

    // Disassemble and send in the envelopes
    Json::Value::Members members(event.getMemberNames());

    for(Json::Value::Members::iterator it = members.begin(); it != members.end(); ++it) {
        std::string key(*it);
        
        std::ostringstream envelope;
        envelope << driver_id << " " << key << " " << m_hostname << " "
                 << std::fixed << std::setprecision(3) << now;

        message.rebuild(envelope.str().length());
        memcpy(message.data(), envelope.str().data(), envelope.str().length());
        s_publisher.send(message, ZMQ_SNDMORE);

        Json::Value object(event[key]);
        std::string value;

        switch(object.type()) {
            case Json::booleanValue:
                value = object.asBool() ? "true" : "false";
                break;
            case Json::intValue:
            case Json::uintValue:
                value = boost::lexical_cast<std::string>(object.asInt());
                break;
            case Json::realValue:
                value = boost::lexical_cast<std::string>(object.asDouble());
                break;
            case Json::stringValue:
                value = object.asString();
                break;
            default:
                value = "<error: non-primitive type>";
        }

        message.rebuild(value.length());
        memcpy(message.data(), value.data(), value.length());
        s_publisher.send(message);
    }
}

void core_t::future(const std::string& future_id, const std::string& key, const Json::Value& value) {
    future_map_t::iterator it(m_futures.find(future_id));
    
    if(it != m_futures.end()) {
        it->second->push(key, value);
    } else {
        syslog(LOG_ERR, "core: [%s()] orphan - part of future %s", __func__, future_id.c_str());
    }
}

void core_t::reap(const std::string& engine_id, const std::string& thread_id) {
    engine_map_t::iterator it(m_engines.find(engine_id));

    if(it != m_engines.end()) {
        it->second->reap(thread_id);
    } else {
        syslog(LOG_ERR, "core: [%s()] orphan - engine %s", __func__, engine_id.c_str());
    }
}

void core_t::seal(const std::string& future_id) {
    future_map_t::iterator it(m_futures.find(future_id));

    if(it == m_futures.end()) {
        syslog(LOG_ERR, "core: [%s()] orphan - future %s", __func__, future_id.c_str());
        return;
    }
        
    std::vector<std::string> route(it->second->route());

    // Send it if it's not an internal future
    if(!route.empty()) {
        zmq::message_t message;
        
        syslog(LOG_DEBUG, "core: sending response - future %s", it->second->id().c_str());

        // Send the identity
        for(std::vector<std::string>::const_iterator id = route.begin(); id != route.end(); ++id) {
            message.rebuild(id->length());
            memcpy(message.data(), id->data(), id->length());
            s_requests.send(message, ZMQ_SNDMORE);
        }
        
        // Send the delimiter
        message.rebuild(0);
        s_requests.send(message, ZMQ_SNDMORE);

        // Append the hostname and send the JSON
        Json::Value root(it->second->root());
        root["hostname"] = m_hostname;

        Json::FastWriter writer;
        std::string json(writer.write(root));
        message.rebuild(json.length());
        memcpy(message.data(), json.data(), json.length());

        s_requests.send(message);
    }

    // Release the future
    m_futures.erase(it);
}

void core_t::recover() {
    // NOTE: Allowing the exception to propagate here, as this is a fatal error
    Json::Value root(storage::storage_t::instance()->all("tasks"));

    if(root.size()) {
        syslog(LOG_NOTICE, "core: loaded %d task(s)", root.size());
        
        // Create a new anonymous future
        future_t* future = new future_t(this, std::vector<std::string>());
        m_futures.insert(future->id(), future);
       
        // Push them as if they were normal requests
        Json::Value::Members hashes(root.getMemberNames());
        future->reserve(hashes);

        for(Json::Value::Members::const_iterator it = hashes.begin(); it != hashes.end(); ++it) {
            std::string hash(*it);
            
            try {
                push(future, hash, root[hash]);
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "core: [%s()] %s", __func__, e.what());
                future->abort(hash, e.what());
            }
        }
    }
}

