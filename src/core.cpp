#include <iomanip>
#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/assign.hpp>

#include "cocaine/core.hpp"
#include "cocaine/future.hpp"
#include "cocaine/plugin.hpp"
#include "cocaine/storage.hpp"

using namespace cocaine::core;
using namespace cocaine::engine;
using namespace cocaine::plugin;

core_t::core_t():
    m_context(1),
    s_events(m_context, ZMQ_PULL),
    s_publisher(m_context, ZMQ_PUB),
    s_requests(m_context, ZMQ_ROUTER),
    s_internal(m_context, ZMQ_PULL)
{
    // Version dump
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);

    syslog(LOG_INFO, "core: using libzmq version %d.%d.%d", major, minor, patch);
    syslog(LOG_INFO, "core: using libev version %d.%d", ev_version_major(), ev_version_minor());
    syslog(LOG_INFO, "core: using libmsgpack version %s", msgpack_version());

    // Fetching the hostname
    char hostname[HOST_NAME_MAX];

    if(gethostname(hostname, HOST_NAME_MAX) == 0) {
        syslog(LOG_INFO, "core: hostname is '%s'", hostname);
        m_hostname = hostname;
    } else {
        throw std::runtime_error("failed to determine the hostname");
    }

    // Internal event sink socket
    s_events.bind("inproc://events");
    e_events.set<core_t, &core_t::event>(this);
    e_events.start(s_events.fd(), EV_READ);

    // Internal future sink socket
    s_internal.bind("inproc://internal");
    e_internal.set<core_t, &core_t::internal>(this);
    e_internal.start(s_internal.fd(), EV_READ);

    // Listening socket
    for(std::vector<std::string>::const_iterator it = config_t::get().net.listen.begin(); it != config_t::get().net.listen.end(); ++it) {
        s_requests.bind(*it);
        syslog(LOG_INFO, "core: listening for requests on %s", it->c_str());
    }

    e_requests.set<core_t, &core_t::request>(this);
    e_requests.start(s_requests.fd(), EV_READ);

    // Publishing socket
#if ZMQ_VERSION > 30000
    s_publisher.setsockopt(ZMQ_SNDHWM, &config_t::get().net.watermark, sizeof(config_t::get().net.watermark));
#else
    s_publisher.setsockopt(ZMQ_HWM, &config_t::get().net.watermark, sizeof(config_t::get().net.watermark));
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

    m_futures.clear();
    m_engines.clear();
    m_histories.clear();

    recover();
}

void core_t::purge(ev::sig& sig, int revents) {
    syslog(LOG_NOTICE, "core: puring the task storage");
    
    m_futures.clear();
    m_engines.clear();
    m_histories.clear();

    try {
        storage::storage_t::instance()->purge("tasks");
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "core: storage failure while purging - %s", e.what());
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
                    throw std::runtime_error("object expected");
                }

                unsigned int version = root.get("version", 1).asUInt();
                std::string token = root.get("token", "").asString();
                
                if(version >= 2) {
                    future->set("protocol", boost::lexical_cast<std::string>(version));
                } else {
                    throw std::runtime_error("outdated protocol version");
                }
      
                if(!token.empty()) {
                    future->set("token", token);

                    if(version > 2) {
                        m_signatures.verify(request,
                            static_cast<const unsigned char*>(signature.data()),
                            signature.size(), token);
                    }
                } else {
                    throw std::runtime_error("security token expected");
                }

                // Request dispatching is performed in this function
                dispatch(future, root); 
            } catch(const std::exception& e) {
                syslog(LOG_ERR, "core: invalid request - %s", e.what());
                future->abort(e.what());
            }
        } else {
            syslog(LOG_ERR, "core: invalid json - %s",
                reader.getFormatedErrorMessages().c_str());
            future->abort(reader.getFormatedErrorMessages());
        }
    }
}

void core_t::dispatch(future_t* future, const Json::Value& root) {
    std::string action = root.get("action", "push").asString();

    if(action == "push" || action == "drop" || action == "history") {
        Json::Value targets = root["targets"];
        Json::Value response;

        if(!targets.isObject() || !targets.size()) {
            throw std::runtime_error("no targets specified");
        }

        // Iterate over all the targets
        Json::Value::Members names = targets.getMemberNames();
        future->reserve(names);

        for(Json::Value::Members::const_iterator it = names.begin(); it != names.end(); ++it) {
            std::string target = *it;

            // Get the target args
            Json::Value args = targets[target];

            // And check if it's an object
            if(!args.isObject()) {
                syslog(LOG_ERR, "core: invalid request - target arguments expected");
                response["error"] = "target arguments expected";
                future->fulfill(target, response);
                continue;
            }

            try {
                if(action == "push") {
                    push(future, target, args);
                } else if(action == "drop") {
                    drop(future, target, args);
                } else if(action == "history") {
                    history(future, target, args);
                }
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "core: error in dispatch() - %s", e.what());
                response["error"] = e.what();
                future->fulfill(target, response);
            } catch(...) {
                syslog(LOG_CRIT, "core: unexpected error in dispatch()");
                abort();
            }
        }
    } else if(action == "stats") {
        stat(future);
    } else {
        throw std::runtime_error("unsupported action");
    }
}

// Built-in commands:
// --------------
// * Push - launches a thread which fetches data from the
//   specified source and publishes it via the PUB socket.
//
// * Drop - shuts down the specified collector.
//   Remaining messages will stay orphaned in the queue,
//   so it's a good idea to drain it after the unsubscription:
//
// * Stats - fetches the current running stats
//
// * History - fetches the event history for the specified subscription key

void core_t::push(future_t* future, const std::string& target, const Json::Value& args) {
    // Check if we have an engine running for the given uri
    engine_map_t::iterator it = m_engines.find(target); 

    if(it == m_engines.end()) {
        // If the engine wasn't found, try to start a new one
        boost::tie(it, boost::tuples::ignore) = m_engines.insert(target,
            new engine_t(m_context, target));
    }

    // Dispatch!
    it->second->push(future, args);
}

void core_t::drop(future_t* future, const std::string& target, const Json::Value& args) {
    engine_map_t::iterator it = m_engines.find(target);
    
    if(it == m_engines.end()) {
        throw std::runtime_error("engine not found");
    }

    // Dispatch!
    it->second->drop(future, args);
}

void core_t::history(future_t* future, const std::string& key, const Json::Value& args) {
    history_map_t::iterator it = m_histories.find(key);

    if(it == m_histories.end()) {
        throw std::runtime_error("history is empty");
    }

    Json::Value result(Json::arrayValue);
    uint32_t depth = args.get("depth", config_t::get().core.history_depth).asUInt(),
             counter = 0;

    for(history_t::const_iterator event = it->second->begin(); event != it->second->end(); ++event) {
        Json::Value object(Json::objectValue);

        for(dict_t::const_iterator pair = event->second.begin(); pair != event->second.end(); ++pair) {
            object["event"][pair->first] = pair->second;
            object["timestamp"] = event->first;
        }

        result.append(object);

        if(++counter == depth) {
            break;
        }
    }

    future->fulfill(key, result);
}

void core_t::stat(future_t* future) {
    Json::Value engines, threads, requests;

    future->reserve(boost::assign::list_of
        ("engines")
        ("threads")
        ("requests")
    );

    for(engine_map_t::const_iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        engines["list"].append(it->first);
    }
    
    engines["total"] = engine::engine_t::objects_created;
    engines["alive"] = engine::engine_t::objects_alive;
    future->fulfill("engines", engines);
    
    threads["total"] = engine::threading::thread_t::objects_created;
    threads["alive"] = engine::threading::thread_t::objects_alive;
    future->fulfill("threads", threads);

    requests["total"] = future_t::objects_created;
    requests["pending"] = future_t::objects_alive;
    future->fulfill("requests", requests);
}

void core_t::seal(const std::string& future_id) {
    future_map_t::iterator it = m_futures.find(future_id);

    if(it == m_futures.end()) {
        syslog(LOG_ERR, "core: found an orphan - future %s", future_id.c_str());
        return;
    }
        
    std::vector<std::string> route = it->second->route();

    // Send it if it's not an internal future
    if(!route.empty()) {
        zmq::message_t message;
        
        syslog(LOG_DEBUG, "core: sending response to '%s' - future %s", 
            it->second->get("token").c_str(), it->second->id().c_str());

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
        Json::Value root = it->second->root();
        root["hostname"] = m_hostname;

        s_requests.send_json(root);
    }

    // Release the future
    m_futures.erase(it);
}

// Publishing format (not JSON, as it will render subscription mechanics pointless):
// ------------------
//   multipart: [key field hostname timestamp] [blob]

void core_t::event(ev::io& io, int revents) {
    ev::tstamp now = m_loop.now();
    zmq::message_t message;
    std::string driver_id;
    dict_t dict;
    
    while(s_events.pending()) {
        // Receive the driver id
        s_events.recv(&message);

        driver_id.assign(
            static_cast<const char*>(message.data()),
            message.size());
    
        // Receive the data
        s_events.recv_packed(dict);

        // Maintain the history for the given driver
        history_map_t::iterator history = m_histories.find(driver_id);

        if(history == m_histories.end()) {
            boost::tie(history, boost::tuples::ignore) = m_histories.insert(driver_id, new history_t());
        } else if(history->second->size() == config_t::get().core.history_depth) {
            history->second->pop_back();
        }
        
        history->second->push_front(std::make_pair(now, dict));

        // Disassemble and send in the envelopes
        for(dict_t::const_iterator it = dict.begin(); it != dict.end(); ++it) {
            std::ostringstream envelope;
            envelope << driver_id << " " << it->first << " " << m_hostname << " "
                     << std::fixed << std::setprecision(3) << now;

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

void core_t::internal(ev::io& io, int revents) {
    while(s_internal.pending()) {
        Json::Value message;
        s_internal.recv_json(message);

        switch(message["command"].asUInt()) {
            case net::FULFILL:
                fulfill(message);
                break;
            case net::SUICIDE:
                reap(message);
                break;
            case net::TRACK:
                // track(message);
                // break;
            default:
                syslog(LOG_ERR, "core: received an unknown internal message");
        }
    }
}

void core_t::fulfill(const Json::Value& message) {
    future_map_t::iterator it = m_futures.find(message["future"].asString());
    
    if(it != m_futures.end()) {
        it->second->fulfill(message["engine"].asString(), message["result"]);
    } else {
        syslog(LOG_ERR, "core: found an orphan - slice for future %s", 
            message["future"].asCString());
    }
}

void core_t::reap(const Json::Value& message) {
    engine_map_t::iterator it = m_engines.find(message["engine"].asString());

    if(it != m_engines.end()) {
        syslog(LOG_DEBUG, "core: termination requested for thread %s in engine %s",
            message["thread"].asCString(), message["engine"].asCString());
        it->second->reap(message["thread"].asString());
    } else {
        syslog(LOG_ERR, "core: found an orphan - engine %s", message["engine"].asCString());
    }
}

void core_t::recover() {
    Json::Value root;
    
    try {
        root = storage::storage_t::instance()->all("tasks");
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "core: storage failure while recovering - %s", e.what());
        return;
    }

    if(root.size()) {
        syslog(LOG_NOTICE, "core: loaded %d task(s)", root.size());
        
        future_t* future = new future_t(this, std::vector<std::string>());
        m_futures.insert(future->id(), future);
        
        Json::Value::Members ids = root.getMemberNames();
        future->reserve(ids);

        for(Json::Value::Members::const_iterator it = ids.begin(); it != ids.end(); ++it) {
            Json::Value object = root[*it];
            future->set("token", object["token"].asString());

            try {
                push(future, object["url"].asString(), object["args"]);
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "core: error in recover() - %s", e.what());
            } catch(...) {
                syslog(LOG_ERR, "core: unexpected error in recover() - %s", e.what());
                abort();
            }
        }
    }
}

