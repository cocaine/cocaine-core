#include <iomanip>
#include <sstream>

#include <boost/lexical_cast.hpp>
#include <boost/assign.hpp>

#include "cocaine/core.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/engine/routing.hpp"
#include "cocaine/future.hpp"
#include "cocaine/response.hpp"
#include "cocaine/storage.hpp"

using namespace cocaine::core;
using namespace cocaine::engine;
using namespace cocaine::plugin;

core_t::core_t():
    m_context(1),
    s_requests(m_context, ZMQ_ROUTER),
    s_publisher(m_context, ZMQ_PUB)
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
}

core_t::~core_t() { }

void core_t::run() {
    recover();
    m_loop.loop();
}

void core_t::terminate(ev::sig& sig, int revents) {
    m_engines.clear();
    m_loop.unloop();
}

void core_t::reload(ev::sig& sig, int revents) {
    syslog(LOG_NOTICE, "core: reloading tasks");

    m_engines.clear();

    try {
        recover();
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "core: [%s()] storage failure - %s", __func__, e.what());
    }
}

void core_t::purge(ev::sig& sig, int revents) {
    syslog(LOG_NOTICE, "core: purging the tasks");
    
    m_engines.clear();

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

            if(!message.size()) {
                break;
            }

            route.push_back(std::string(
                static_cast<const char*>(message.data()),
                message.size()));
        }

        // Create a response
        boost::shared_ptr<response_t> response(
            new response_t(route, shared_from_this()));
        
        // Receive the request
        s_requests.recv(&message);

        request.assign(static_cast<const char*>(message.data()),
            message.size());

        // Receive the signature, if it's there
        signature.rebuild();

        if(s_requests.has_more()) {
            s_requests.recv(&signature);
        }

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
                dispatch(response, root);
            } catch(const std::exception& e) {
                syslog(LOG_ERR, "core: [%s()] %s", __func__, e.what());
                response->abort(e.what());
            }
        } else {
            syslog(LOG_ERR, "core: [%s()] %s", __func__,
                reader.getFormatedErrorMessages().c_str());
            response->abort(reader.getFormatedErrorMessages());
        }
    }
}

void core_t::dispatch(boost::shared_ptr<response_t> response, const Json::Value& root) {
    std::string action(root["action"].asString());

    if(action == "push" || action == "once" || action == "drop" || action == "past") {
        Json::Value targets(root["targets"]);

        if(!targets.isObject() || !targets.size()) {
            throw std::runtime_error("no targets has been specified");
        }

        // Iterate over all the targets
        Json::Value::Members names(targets.getMemberNames());

        for(Json::Value::Members::iterator it = names.begin(); it != names.end(); ++it) {
            // Get the target and args
            std::string target(*it);
            Json::Value args(targets[target]);

            // Invoke the handler
            try {
                if(args.isObject()) {
                    if(action == "push") {
                        response->wait(target, push(args));
                    } else if(action == "once") {
                        response->wait(target, once(args));
                    } else if(action == "drop") {
                        response->wait(target, drop(args));
                    } else if(action == "past") {
                        response->push(target, past(args));
                    }
                } else {
                    throw std::runtime_error("arguments expected");
                }
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "core: [%s()] %s", __func__, e.what());
                response->abort(target, e.what());
            }
        }
    } else if(action == "stats") {
        stats(response);
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

boost::shared_ptr<future_t> core_t::push(const Json::Value& args) {
    static std::map<const std::string, unsigned int> types = boost::assign::map_list_of
        ("auto", AUTO)
        ("manual", MANUAL)
        ("fs", FILESYSTEM)
        ("sink", SINK)
        ("server", SERVER);

    std::string uri(args["uri"].asString()),
                type(args["driver"].asString());
    engine_map_t::iterator it(m_engines.find(uri)); 
    
    if(uri.empty()) {
        throw std::runtime_error("no source uri has been specified");
    } else if(types.find(type) == types.end()) {
        throw std::runtime_error("invalid driver type");
    } else if(it == m_engines.end()) {
        boost::tie(it, boost::tuples::ignore) = m_engines.insert(uri,
            new engine_t(m_context, shared_from_this(), uri));
    }

    return it->second->cast(
        routing::specific_thread(
            args["threads"].isNull() ? it->second->id() : args["thread"].asString()),
        boost::make_tuple(
            PUSH,
            types[type],
            args));
}

boost::shared_ptr<future_t> core_t::once(const Json::Value& args) {
    std::string uri(args["uri"].asString());
    engine_map_t::iterator it(m_engines.find(uri)); 
    
    if(uri.empty()) {
        throw std::runtime_error("no source uri has been specified");
    } else if(it == m_engines.end()) {
        boost::tie(it, boost::tuples::ignore) = m_engines.insert(uri,
            new engine_t(m_context, shared_from_this(), uri));
    }

    return it->second->cast(
        routing::shortest_queue(
            args.get("queue", config_t::get().engine.queue_depth).asUInt()),
        boost::make_tuple(
            ONCE,
            args));
}

boost::shared_ptr<future_t> core_t::drop(const Json::Value& args) {
    std::string uri(args["uri"].asString()),
                key(args["key"].asString());
    engine_map_t::iterator it(m_engines.find(uri));
        
    if(uri.empty()) {
        throw std::runtime_error("no source uri has been specified");
    } else if(key.empty()) {
        throw std::runtime_error("no driver id has been specified");
    } else if(it == m_engines.end()) {
        throw std::runtime_error("the specified engine is not active");
    }

    return it->second->cast(
        routing::specific_thread(
            args["threads"].isNull() ? it->second->id() : args["thread"].asString()),
        boost::make_tuple(
            DROP,
            key));
}

Json::Value core_t::past(const Json::Value& args) {
    std::string key(args["key"].asString());

    if(key.empty()) {
        throw std::runtime_error("no driver id has been specified");
    }

    history_map_t::iterator it(m_histories.find(key));

    if(it == m_histories.end()) {
        throw std::runtime_error("the past for a given key is empty");
    }

    uint32_t depth = args.get("depth", config_t::get().core.history_depth).asUInt(),
             counter = 0;
    Json::Value result(Json::arrayValue);

    for(history_t::const_iterator event = it->second->begin(); event != it->second->end(); ++event) {
        Json::Value object(Json::objectValue);

        object["timestamp"] = event->first;
        object["event"] = event->second;

        result.append(object);

        if(++counter == depth) {
            break;
        }
    }

    return result;
}

void core_t::stats(boost::shared_ptr<response_t> response) {
    Json::Value engines(Json::objectValue),
                threads(Json::objectValue),
                requests(Json::objectValue);

    engines["total"] = engine::engine_t::objects_created;
    
    for(engine_map_t::const_iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        engines["alive"].append(it->first);
    }
    
    threads["total"] = engine::thread_t::objects_created;
    threads["alive"] = engine::thread_t::objects_alive;

    requests["total"] = response_t::objects_created;
    requests["pending"] = response_t::objects_alive;

    response->push("engines", engines);
    response->push("threads", threads);
    response->push("requests", requests);
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

void core_t::seal(response_t* response) {
    std::vector<std::string> route(response->route());
    zmq::message_t message;
    
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
    Json::Value root(response->root());
    root["hostname"] = m_hostname;

    Json::FastWriter writer;
    std::string json(writer.write(root));
    message.rebuild(json.length());
    memcpy(message.data(), json.data(), json.length());

    s_requests.send(message);
}

void core_t::recover() {
    // NOTE: Allowing the exception to propagate here, as this is a fatal error
    Json::Value root(storage::storage_t::instance()->all("tasks"));

    if(root.size()) {
        syslog(LOG_NOTICE, "core: loaded %d task(s)", root.size());
       
        Json::Value::Members hashes(root.getMemberNames());

        for(Json::Value::Members::const_iterator it = hashes.begin(); it != hashes.end(); ++it) {
            std::string hash(*it);
            
            try {
                push(root[hash]);
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "core: [%s()] %s", __func__, e.what());
            }
        }
    }
}

