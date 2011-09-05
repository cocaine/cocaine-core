#include <iomanip>
#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple.hpp>

#include "core.hpp"
#include "future.hpp"
#include "plugin.hpp"
#include "storage.hpp"

using namespace yappi::core;
using namespace yappi::engine;
using namespace yappi::plugin;

core_t::core_t():
    m_context(1),
    s_events(m_context, ZMQ_PULL),
    s_publisher(m_context, ZMQ_PUB),
    s_requests(m_context, ZMQ_ROUTER),
    s_futures(m_context, ZMQ_PULL),
    s_reaper(m_context, ZMQ_PULL)
{
    // Version dump
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);

    syslog(LOG_INFO, "core: using libzmq version %d.%d.%d", major, minor, patch);
    syslog(LOG_INFO, "core: using libev version %d.%d", ev_version_major(), ev_version_minor());
    syslog(LOG_INFO, "core: using libmsgpack version %s", msgpack_version());

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
    m_futures.clear();
    m_engines.clear();
    m_histories.clear();

    if(config_t::get().storage.disabled) {
        return;
    }

    storage::storage_t::instance()->purge();
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
                
                if(version >= config_t::get().core.protocol) {
                    future->set("protocol", boost::lexical_cast<std::string>(version));
                } else {
                    throw std::runtime_error("outdated protocol version");
                }
      
                if(!token.empty()) {
                    future->set("token", token);

                    if(version > 2) {
                        security::signatures_t::instance()->verify(request,
                            static_cast<const unsigned char*>(signature.data()),
                            signature.size(), token);
                    }
                } else {
                    throw std::runtime_error("security token expected");
                }

                dispatch(future, root); 
            } catch(const std::exception& e) {
                syslog(LOG_ERR, "core: invalid request - %s", e.what());
                future->fulfill("error", e.what());
            }
        } else {
            syslog(LOG_ERR, "core: invalid json - %s",
                reader.getFormatedErrorMessages().c_str());
            future->fulfill("error", reader.getFormatedErrorMessages());
        }
    }
}

void core_t::dispatch(future_t* future, const Json::Value& root) {
    std::string action = root.get("action", "push").asString();

    if(action == "push" || action == "drop" || action == "history") {
        Json::Value targets = root["targets"];

        if(!targets.isObject() || !targets.size()) {
            throw std::runtime_error("no targets specified");
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
                syslog(LOG_ERR, "core: invalid request - target arguments expected");
                response["error"] = "target arguments expected";
                future->fulfill(target, response);
                continue;
            }

            if(action == "push") {
                push(future, target, args);
            } else if(action == "drop") {
                drop(future, target, args);
            } else if(action == "history") {
                history(future, target, args);
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
    Json::Value response;

    // Check if we have an engine running for the given uri
    engine_map_t::iterator it = m_engines.find(target); 

    if(it == m_engines.end()) {
        try {
            // If the engine wasn't found, try to start a new one
            std::auto_ptr<engine_t> engine(new engine_t(m_context, target));
            boost::tie(it, boost::tuples::ignore) = m_engines.insert(target, engine);
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "core: runtime error in push() - %s", e.what());
            response["error"] = e.what();
            future->fulfill(target, response);
            return;
        } catch(...) {
            syslog(LOG_CRIT, "core: unexpected exception in push()");
            abort();
        }
    }

    // Dispatch!
    it->second->push(future, args);
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
    it->second->drop(future, args);
}

void core_t::stat(future_t* future) {
    Json::Value engines, threads, requests;

    future->await(3);

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

void core_t::history(future_t* future, const std::string& key, const Json::Value& args) {
    history_map_t::iterator it = m_histories.find(key);

    if(it == m_histories.end()) {
        Json::Value response;
        response["error"] = "history is empty";
        future->fulfill(key, response);
        return;
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

        // Send the JSON
        s_requests.send_json(it->second->root());
    }

    // Release the future
    m_futures.erase(it);
}

// Publishing format (not JSON, as it will render subscription mechanics pointless):
// ------------------
//   multipart: [key field timestamp] [blob]

void core_t::event(ev::io& io, int revents) {
    zmq::message_t message;
    std::string driver_id;
    dict_t dict;
    ev::tstamp now = m_loop.now();
    
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
            std::auto_ptr<history_t> deque(new history_t());
            boost::tie(history, boost::tuples::ignore) = m_histories.insert(driver_id, deque);
        } else if(history->second->size() == config_t::get().core.history_depth) {
            history->second->pop_back();
        }
        
        history->second->push_front(std::make_pair(now, dict));

        // Disassemble and send in the envelopes
        for(dict_t::const_iterator it = dict.begin(); it != dict.end(); ++it) {
            std::ostringstream envelope;
            envelope << driver_id << " " << it->first << " "
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

void core_t::future(ev::io& io, int revents) {
    while(s_futures.pending()) {
        Json::Value message;
        s_futures.recv_json(message);

        future_map_t::iterator it = m_futures.find(message["future"].asString());
        
        if(it == m_futures.end()) {
            syslog(LOG_ERR, "core: found an orphan - slice for future %s", 
                message["future"].asCString());
            continue;
        }

        it->second->fulfill(message["engine"].asString(), message["result"]);
    }
}

void core_t::reap(ev::io& io, int revents) {
    while(s_reaper.pending()) {
        Json::Value message;
        s_reaper.recv_json(message);

        engine_map_t::iterator it = m_engines.find(message["engine"].asString());

        if(it == m_engines.end()) {
            syslog(LOG_ERR, "core: found an orphan - engine %s", message["engine"].asCString());
            continue;
        }
        
        syslog(LOG_DEBUG, "core: suicide requested for thread %s in engine %s",
            message["thread"].asCString(), message["engine"].asCString());

        it->second->reap(message["thread"].asString());
    }
}

void core_t::recover() {
    if(config_t::get().storage.disabled) {
        return;
    }

    Json::Value root = storage::storage_t::instance()->all();

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

