#include <boost/algorithm/string/join.hpp>

#include "cocaine/core.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/storages/abstract.hpp"

using namespace cocaine::core;
using namespace cocaine::engine;
using namespace cocaine::lines;
using namespace cocaine::storage;

core_t::core_t():
    m_context(1),
    m_server(m_context, ZMQ_ROUTER)
{
    // Version dump
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);

    syslog(LOG_INFO, "core: using libev version %d.%d", ev_version_major(), ev_version_minor());
    syslog(LOG_INFO, "core: using libmsgpack version %s", msgpack_version());
    syslog(LOG_INFO, "core: using libzmq version %d.%d.%d", major, minor, patch);

    // Fetching the hostname
    char hostname[256];

    if(gethostname(hostname, 256) == 0) {
        config_t::set().core.hostname = hostname;
    } else {
        throw std::runtime_error("failed to determine the hostname");
    }

    // Listening socket
    std::string route(
        config_t::get().core.hostname + "/" + 
        config_t::get().core.instance);
    m_server.setsockopt(ZMQ_IDENTITY, route.data(), route.length());
    
    syslog(LOG_INFO, "core: route to this node is '%s'", route.c_str());

    for(std::vector<std::string>::const_iterator it = config_t::get().core.endpoints.begin();
        it != config_t::get().core.endpoints.end();
        ++it) 
    {
        m_server.bind(*it);
        syslog(LOG_INFO, "core: listening for requests on %s", it->c_str());
    }

    m_watcher.set<core_t, &core_t::request>(this);
    m_watcher.start(m_server.fd(), EV_READ);
    m_processor.set<core_t, &core_t::process>(this);

    // Initialize signal watchers
    m_sigint.set<core_t, &core_t::terminate>(this);
    m_sigint.start(SIGINT);

    m_sigterm.set<core_t, &core_t::terminate>(this);
    m_sigterm.start(SIGTERM);

    m_sigquit.set<core_t, &core_t::terminate>(this);
    m_sigquit.start(SIGQUIT);

    m_sighup.set<core_t, &core_t::reload>(this);
    m_sighup.start(SIGHUP);
}

core_t::~core_t() {
    syslog(LOG_DEBUG, "core: destructing");
}

void core_t::start() {
    recover();
    ev::get_default_loop().loop();
}

void core_t::terminate(ev::sig& sig, int revents) {
    syslog(LOG_NOTICE, "core: stopping the engines");

    for(engine_map_t::iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        it->second->stop();
    }

    m_engines.clear();

    ev::get_default_loop().unloop(ev::ALL);
}

void core_t::reload(ev::sig& sig, int revents) {
    syslog(LOG_NOTICE, "core: reloading apps");

    for(engine_map_t::iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        it->second->stop();
    }

    m_engines.clear();

    try {
        recover();
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "core: failed to reload apps due to the storage failure - %s", e.what());
    }
}

void core_t::request(ev::io& w, int revents) {
    if(m_server.pending()) {
        m_watcher.stop();
        m_processor.start();
    }
}

void core_t::process(ev::idle& w, int revents) {
    if(m_server.pending()) {
        zmq::message_t message, signature;
        route_t route;

        Json::Reader reader(Json::Features::strictMode());
        Json::Value root;

        while(true) {
            m_server.recv(&message);

#if ZMQ_VERSION > 30000
            if(!m_server.label()) {
#else
            if(!message.size()) {
#endif
                break;
            }

            route.push_back(std::string(
                static_cast<const char*>(message.data()),
                message.size()));
        }

#if ZMQ_VERSION < 30000
        m_server.recv(&message);
#endif

        if(m_server.more()) {
            m_server.recv(&signature);
        }

        // Parse the request
        root.clear();

        if(reader.parse(
            static_cast<const char*>(message.data()),
            static_cast<const char*>(message.data()) + message.size(),
            root)) 
        {
            try {
                if(!root.isObject()) {
                    throw std::runtime_error("json root must be an object");
                }

                unsigned int version = root["version"].asUInt();
                std::string username(root["username"].asString());
                
                if(version < 2) {
                    throw std::runtime_error("outdated protocol version");
                }
      
                if(version >= 3) {
                    if(!username.empty()) {
                        m_signatures.verify(
                            static_cast<const char*>(message.data()),
                            message.size(),
                            static_cast<const unsigned char*>(signature.data()),
                            signature.size(),
                            username);
                    } else {
                        throw std::runtime_error("username expected");
                    }
                }

                respond(route, dispatch(root));
            } catch(const std::exception& e) {
                respond(route, helpers::make_json("error", e.what()));
            }
        } else {
            respond(route, helpers::make_json("error", reader.getFormatedErrorMessages()));
        }
    } else {
        m_watcher.start(m_server.fd(), ev::READ);
        m_processor.stop();
    }
}

Json::Value core_t::dispatch(const Json::Value& root) {
    std::string action(root["action"].asString());

    if(action == "create") {
        Json::Value apps(root["apps"]), result(Json::objectValue);

        if(!apps.isObject() || !apps.size()) {
            throw std::runtime_error("no apps has been specified");
        }

        // Iterate over all the apps
        Json::Value::Members names(apps.getMemberNames());

        for(Json::Value::Members::iterator it = names.begin(); it != names.end(); ++it) {
            // Get the app name and app manifest
            std::string app(*it);
            Json::Value manifest(apps[app]);

            // Invoke the handler
            try {
                if(manifest.isObject()) {
                    result[app] = create_engine(app, manifest);
                } else {
                    throw std::runtime_error("app manifest expected");
                }
            } catch(const std::runtime_error& e) {
                result[app]["error"] = e.what();
            } catch(const zmq::error_t& e) {
                result[app]["error"] = e.what();
            }
        }

        return result;
    } else if(action == "delete") {
        Json::Value apps(root["apps"]), result(Json::objectValue);

        if(!apps.isArray() || !apps.size()) {
            throw std::runtime_error("no apps has been specified");
        }

        for(Json::Value::iterator it = apps.begin(); it != apps.end(); ++it) {
            std::string app((*it).asString());
            
            try {
                result[app] = delete_engine(app);
            } catch(const std::runtime_error& e) {
                result[app]["error"] = e.what();
            }
        }

        return result;
    } else if(action == "info") {
        return info();
    } else {
        throw std::runtime_error("unsupported action");
    }
}

// Commands
// --------

Json::Value core_t::create_engine(const std::string& name, const Json::Value& manifest) {
    if(m_engines.find(name) != m_engines.end()) {
        throw std::runtime_error("the specified app is already active");
    }

    // Launch the engine
    std::auto_ptr<engine_t> engine(new engine_t(m_context, name));
    Json::Value result(engine->start(manifest));

    try {
        storage_t::create()->put("apps", name, manifest);
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "core: failed to create '%s' engine due to the storage failure - %s",
            name.c_str(), e.what());
        engine->stop();
        throw;
    }

    // Only leave the engine running if all of the above succeded
    m_engines.insert(name, engine);
    
    return result;
}

Json::Value core_t::delete_engine(const std::string& name) {
    engine_map_t::iterator engine(m_engines.find(name));

    if(engine == m_engines.end()) {
        throw std::runtime_error("the specified app is not active");
    }

    try {
        storage_t::create()->remove("apps", name);
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "core: failed to destroy '%s' engine due to the storage failure - %s",
            name.c_str(), e.what());
        throw;
    }

    Json::Value result(engine->second->stop());
    m_engines.erase(engine);

    return result;
}

Json::Value core_t::info() const {
    Json::Value result(Json::objectValue);

    for(engine_map_t::const_iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        result["apps"][it->first] = it->second->info();
    }

    result["events"]["pending"] = deferred_t::objects_alive;
    result["events"]["processed"] = deferred_t::objects_created;
    
    result["sockets"] = socket_t::objects_alive;
    result["workers"] = backend_t::objects_alive;
   
    return result;
}

void core_t::respond(const route_t& route, const Json::Value& object) {
    zmq::message_t message;
    
    // Send the identity
    for(route_t::const_iterator id = route.begin(); id != route.end(); ++id) {
        message.rebuild(id->length());
        memcpy(message.data(), id->data(), id->length());
#if ZMQ_VERSION < 30000
        m_server.send(message, ZMQ_SNDMORE);
#else
        m_server.send(message, ZMQ_SNDMORE | ZMQ_SNDLABEL);
#endif
    }

#if ZMQ_VERSION < 30000                
    // Send the delimiter
    message.rebuild(0);
    m_server.send(message, ZMQ_SNDMORE);
#endif

    Json::FastWriter writer;
    std::string json(writer.write(object));
    message.rebuild(json.length());
    memcpy(message.data(), json.data(), json.length());

    m_server.send(message);
}


void core_t::recover() {
    // NOTE: Allowing the exception to propagate here, as this is a fatal error.
    Json::Value root(storage_t::create()->all("apps"));

    if(root.size()) {
        Json::Value::Members apps(root.getMemberNames());
        
        syslog(LOG_NOTICE, "core: recovering %d %s: %s", root.size(),
            root.size() == 1 ? "app" : "apps", boost::algorithm::join(apps, ", ").c_str());
        
        for(Json::Value::Members::iterator it = apps.begin(); it != apps.end(); ++it) {
            std::string app(*it);
            
            // NOTE: Intentionally not catching anything here too.
            create_engine(app, root[app]);
        }
    }
}

