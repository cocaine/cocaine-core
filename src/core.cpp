#include "cocaine/core.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/future.hpp"
#include "cocaine/security/digest.hpp"
#include "cocaine/storage.hpp"

using namespace cocaine::core;
using namespace cocaine::engine;
using namespace cocaine::plugin;
using namespace cocaine::security;
using namespace cocaine::storage;

core_t::core_t():
    m_context(1),
    m_server(m_context, ZMQ_ROUTER)
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
        config_t::set().core.hostname = hostname;
    } else {
        throw std::runtime_error("failed to determine the hostname");
    }

    // Listening socket
    config_t::set().core.route =
        config_t::get().core.hostname + "/" +
        config_t::get().core.instance;
    
    syslog(LOG_INFO, "core: route to this node is '%s'", config_t::get().core.route.c_str());

    for(std::vector<std::string>::const_iterator it = config_t::get().core.endpoints.begin(); it != config_t::get().core.endpoints.end(); ++it) {
        m_server.bind(*it);
        syslog(LOG_INFO, "core: listening for requests on %s", it->c_str());
    }

    m_request_watcher.set<core_t, &core_t::request>(this);
    m_request_watcher.start(m_server.fd(), EV_READ);
    m_request_processor.set<core_t, &core_t::process_request>(this);
    m_request_processor.start();

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

// FIXME: Why the hell is this needed anyway?
core_t::~core_t() { }

void core_t::run() {
    recover();
    ev::get_default_loop().loop();
}

void core_t::terminate(ev::sig& sig, int revents) {
    for(engine_map_t::iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        it->second->stop();
    }

    m_engines.clear();
    
    ev::get_default_loop().unloop();
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
        syslog(LOG_ERR, "core: [%s()] storage failure - %s", __func__, e.what());
    }
}

void core_t::request(ev::io& w, int revents) {
    if(m_server.pending() && !m_request_processor.is_active()) {
        m_request_processor.start();
    }
}

void core_t::process_request(ev::idle& w, int revents) {
    if(m_server.pending()) {
        zmq::message_t message, signature(0);
        lines::route_t route;
        std::string request;

        Json::Reader reader(Json::Features::strictMode());
        Json::Value root;

        while(true) {
            m_server.recv(&message);

#if ZMQ_VERSION > 30000
            if(!m_server.is_label()) {
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

        // Create a response
        boost::shared_ptr<lines::response_t> response(
            new lines::response_t(route, shared_from_this()));
        
        request.assign(static_cast<const char*>(message.data()),
            message.size());

        if(m_server.has_more()) {
            m_server.recv(&signature);
        }

        // Parse the request
        root.clear();

        if(reader.parse(request, root)) {
            try {
                if(!root.isObject()) {
                    throw std::runtime_error("json root must be an object");
                }

                unsigned int version = root["version"].asUInt();
                std::string username(root["username"].asString());
                
                if(version < 2) {
                    throw std::runtime_error("outdated protocol version");
                }
      
                if(!username.empty()) {
                    if(version >= 3) {
                        m_signatures.verify(request,
                            static_cast<const unsigned char*>(signature.data()),
                            signature.size(), username);
                    }
                } else {
                    throw std::runtime_error("username expected");
                }

                dispatch(response, root);
            } catch(const std::exception& e) {
                syslog(LOG_ERR, "core: [%s()] %s", __func__, e.what());
                response->abort(e.what());
            }
        } else {
            syslog(LOG_ERR, "core: [%s()] malformed json - %s", __func__,
                reader.getFormatedErrorMessages().c_str());
            response->abort(reader.getFormatedErrorMessages());
        }
    } else {
        m_request_processor.stop();
    }
}

void core_t::dispatch(boost::shared_ptr<lines::response_t> response, const Json::Value& root) {
    std::string action(root["action"].asString());

    if(action == "create" || action == "delete") {
        Json::Value apps(root["apps"]);

        if(!apps.isObject() || !apps.size()) {
            throw std::runtime_error("no apps has been defined");
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
                    if(action == "create") {
                        response->push(app, create_engine(app, manifest));
                    } else if(action == "delete") {
                        response->push(app, delete_engine(app));
                    }
                } else {
                    throw std::runtime_error("app manifest expected");
                }
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "core: [%s()] %s", __func__, e.what());
                response->abort(app, e.what());
            } catch(const zmq::error_t& e) {
                syslog(LOG_ERR, "core: [%s()] network error - %s", __func__, e.what());
                response->abort(app, e.what());
            }
        }
    } else if(action == "statistics") {
        response->push("", stats());
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
    boost::shared_ptr<engine_t> engine(new engine_t(m_context, name));
    Json::Value result(engine->run(manifest));

    try {
        storage_t::instance()->put("apps", name, manifest);
    } catch(const std::runtime_error& e) {
        engine->stop();
        throw;
    }

    // Only leave the engine running if all of the above succeded
    m_engines.insert(std::make_pair(name, engine));
    
    return result;
}

Json::Value core_t::delete_engine(const std::string& name) {
    engine_map_t::iterator engine(m_engines.find(name));

    if(engine == m_engines.end()) {
        throw std::runtime_error("the specified app is not active");
    }

    storage_t::instance()->remove("apps", name);

    Json::Value result(engine->second->stop());
    m_engines.erase(engine);

    return result;
}

Json::Value core_t::stats() {
    Json::Value result;

    for(engine_map_t::const_iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        result["apps"][it->first] = it->second->stats();
    }
    
    result["threads:total"] = engine::thread_t::objects_created;

    result["requests:total"] = lines::response_t::objects_created;
    result["requests:pending"] = lines::response_t::objects_alive;

    result["publications:total"] = lines::publication_t::objects_created;
    result["publications:pending"] = lines::publication_t::objects_alive;
    
    return result;
}

void core_t::respond(const lines::route_t& route, const Json::Value& object) {
    zmq::message_t message;
    
    // Send the identity
    for(lines::route_t::const_iterator id = route.begin(); id != route.end(); ++id) {
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
    // NOTE: Allowing the exception to propagate here, as this is a fatal error
    Json::Value root(storage_t::instance()->all("apps"));

    if(root.size()) {
        syslog(LOG_NOTICE, "core: loaded %d apps(s)", root.size());
       
        Json::Value::Members apps(root.getMemberNames());

        for(Json::Value::Members::iterator it = apps.begin(); it != apps.end(); ++it) {
            std::string app(*it);
            
            try {
                create_engine(app, root[app]);
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "core: [%s()] %s", __func__, e.what());
            } catch(const zmq::error_t& e) {
                syslog(LOG_ERR, "core: [%s()] network error - %s", __func__, e.what());
            }
        }
    }
}

