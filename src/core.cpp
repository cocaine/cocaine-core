#include "cocaine/core.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/future.hpp"
#include "cocaine/response.hpp"
#include "cocaine/storage.hpp"

using namespace cocaine::core;
using namespace cocaine::engine;
using namespace cocaine::plugin;
using namespace cocaine::storage;

core_t::core_t():
    m_context(1)
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
    s_requests.reset(new lines::socket_t(m_context, ZMQ_ROUTER,
        config_t::get().core.hostname + "/" + config_t::get().core.instance));
    
    for(std::vector<std::string>::const_iterator it = config_t::get().net.listen.begin(); it != config_t::get().net.listen.end(); ++it) {
        s_requests->bind(*it);
        syslog(LOG_INFO, "core: listening for requests on %s", it->c_str());
    }

    e_requests.set<core_t, &core_t::request>(this);
    e_requests.start(s_requests->fd(), EV_READ);

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

// XXX: Why the hell is this needed anyway?
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
    syslog(LOG_NOTICE, "core: reloading tasks");

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

void core_t::purge(ev::sig& sig, int revents) {
    syslog(LOG_NOTICE, "core: purging the tasks");
    
    for(engine_map_t::iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        it->second->stop();
    }

    m_engines.clear();
    
    try {
        storage_t::instance()->purge("tasks");
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

    while((revents & ev::READ) && s_requests->pending()) {
        route.clear();

        while(true) {
            s_requests->recv(&message);

            if(!message.size()) {
                break;
            }

            route.push_back(std::string(
                static_cast<const char*>(message.data()),
                message.size()));
        }

        // Create a response
        boost::shared_ptr<response_t> response(
            new response_t(route, s_requests));
        
        // Receive the request
        s_requests->recv(&message);

        request.assign(static_cast<const char*>(message.data()),
            message.size());

        // Receive the signature, if it's there
        signature.rebuild();

        if(s_requests->has_more()) {
            s_requests->recv(&signature);
        }

        // Parse the request
        root.clear();

        if(reader.parse(request, root)) {
            try {
                if(!root.isObject()) {
                    throw std::runtime_error("root object expected");
                }

                unsigned int version = root["version"].asUInt();
                std::string username(root["username"].asString());
                
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

    if(action == "create" || action == "delete") {
        Json::Value apps(root["apps"]);

        if(!apps.isObject() || !apps.size()) {
            throw std::runtime_error("no apps has been defined");
        }

        // Iterate over all the apps
        Json::Value::Members names(apps.getMemberNames());

        for(Json::Value::Members::iterator it = names.begin(); it != names.end(); ++it) {
            // Get the app name and args
            std::string app(*it);
            Json::Value args(apps[app]);

            // Invoke the handler
            try {
                if(args.isObject()) {
                    if(action == "create") {
                        response->push(app, create_engine(args));
                    } else if(action == "delete") {
                        response->push(app, delete_engine(args));
                    }
                } else {
                    throw std::runtime_error("arguments expected");
                }
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "core: [%s()] %s", __func__, e.what());
                response->abort(app, e.what());
            }
        }
    } else if(action == "stats") {
        response->push("stats", stats());
    } else {
        throw std::runtime_error("unsupported action");
    }
}

// Commands
// --------

Json::Value core_t::create_engine(const Json::Value& args) {
    std::string uri(args["uri"].asString());
    Json::Value result;

    if(uri.empty()) {
        throw std::runtime_error("no app uri has been specified");
    } else if(m_engines.find(uri) != m_engines.end()) {
        throw std::runtime_error("the specified app is already active");
    }

    boost::shared_ptr<engine_t> engine(new engine_t(m_context, uri));

    result = engine->run(args);
    m_engines.insert(std::make_pair(uri, engine));

    return result;
}

Json::Value core_t::delete_engine(const Json::Value& args) {
    std::string uri(args["uri"].asString());
    engine_map_t::iterator engine(m_engines.find(uri));
    Json::Value result;

    if(uri.empty()) {
        throw std::runtime_error("no app uri has been specified");
    } else if(engine == m_engines.end()) {
        throw std::runtime_error("the specified app is not active");
    }

    engine->second->stop();
    m_engines.erase(engine);
    result["status"] = "stopped";

    return result;
}

Json::Value core_t::stats() {
    Json::Value result;

    result["engines"]["total"] = engine::engine_t::objects_created;
    
    for(engine_map_t::const_iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        result["engines"]["alive"].append(it->first);
    }
    
    result["threads"]["total"] = engine::thread_t::objects_created;
    result["threads"]["alive"] = engine::thread_t::objects_alive;

    result["requests"]["total"] = response_t::objects_created;
    result["requests"]["pending"] = response_t::objects_alive;

    return result;
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
                // push(root[app]);
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "core: [%s()] %s", __func__, e.what());
            }
        }
    }
}

