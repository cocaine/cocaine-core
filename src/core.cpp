//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <sstream>

#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>

#include "cocaine/context.hpp"
#include "cocaine/core.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/storages/base.hpp"

using namespace cocaine;
using namespace cocaine::core;
using namespace cocaine::engine;
using namespace cocaine::networking;
using namespace cocaine::storage;

core_t::core_t(const config_t& config):
    m_context(config),
    m_auth(m_context),
    m_server(*m_context.bus, ZMQ_ROUTER, boost::algorithm::join(
        boost::assign::list_of
            (m_context.config.core.instance)
            (m_context.config.core.hostname),
        "/")
    ),
    m_birthstamp(ev::get_default_loop().now())
{
    // Information
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);

    syslog(LOG_INFO, "core: using libev version %d.%d", ev_version_major(), ev_version_minor());
    syslog(LOG_INFO, "core: using libmsgpack version %s", msgpack_version());
    syslog(LOG_INFO, "core: using libzmq version %d.%d.%d", major, minor, patch);
    syslog(LOG_INFO, "core: route to this node is '%s'", m_server.route().c_str());

    // Listening socket
    int linger = 0;

    m_server.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    for(std::vector<std::string>::const_iterator it = m_context.config.core.endpoints.begin();
        it != m_context.config.core.endpoints.end();
        ++it) 
    {
        m_server.bind(*it);
        syslog(LOG_INFO, "core: listening on %s", it->c_str());
    }

    // Automatic discovery support
    if(!m_context.config.core.announce_endpoint.empty()) {
        try {
            m_announces.reset(new socket_t(*m_context.bus, ZMQ_PUB));
            m_announces->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
            m_announces->connect("epgm://" + m_context.config.core.announce_endpoint);
        } catch(const zmq::error_t& e) {
            throw std::runtime_error(std::string("invalid announce endpoint - ") + e.what());
        }

        syslog(LOG_INFO, "core: announcing on %s", m_context.config.core.announce_endpoint.c_str());

        m_announce_timer.reset(new ev::timer());
        m_announce_timer->set<core_t, &core_t::announce>(this);
        m_announce_timer->start(0.0f, m_context.config.core.announce_interval);
    }

    m_watcher.set<core_t, &core_t::request>(this);
    m_watcher.start(m_server.fd(), ev::READ);
    m_processor.set<core_t, &core_t::process>(this);
    m_pumper.set<core_t, &core_t::pump>(this);
    m_pumper.start(0.2f, 0.2f);    

    // Signal watchers
    m_sigint.set<core_t, &core_t::terminate>(this);
    m_sigint.start(SIGINT);

    m_sigterm.set<core_t, &core_t::terminate>(this);
    m_sigterm.start(SIGTERM);

    m_sigquit.set<core_t, &core_t::terminate>(this);
    m_sigquit.start(SIGQUIT);

    m_sighup.set<core_t, &core_t::reload>(this);
    m_sighup.start(SIGHUP);
    
    // Recovering the saved state
    recover();
}

core_t::~core_t() {
    syslog(LOG_DEBUG, "core: destructing");
}

void core_t::loop() {
    ev::get_default_loop().loop();
}

void core_t::terminate(ev::sig&, int) {
    syslog(LOG_NOTICE, "core: stopping the engines");

    m_engines.clear();

    ev::get_default_loop().unloop(ev::ALL);
}

void core_t::reload(ev::sig&, int) {
    syslog(LOG_NOTICE, "core: reloading apps");

    m_engines.clear();

    try {
        recover();
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "core: unable to reload apps due to the storage failure - %s", e.what());
    }
}

void core_t::request(ev::io&, int) {
    if(m_server.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void core_t::process(ev::idle&, int) {
    if(m_server.pending()) {
        zmq::message_t message, signature;
        route_t route;

        Json::Reader reader(Json::Features::strictMode());
        Json::Value root;

        do {
            BOOST_VERIFY(m_server.recv(&message));

            if(!message.size()) {
                break;
            }

            route.push_back(
                std::string(
                    static_cast<const char*>(message.data()),
                    message.size()
                )
            );
        } while(m_server.more());

        if(route.empty() || !m_server.more()) {
            syslog(LOG_ERR, "core: got a corrupted request - invalid route");
            return;
        }

        BOOST_VERIFY(m_server.recv(&message));

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
                
                if(version < 2 || version > 3) {
                    throw std::runtime_error("unsupported protocol version");
                }
      
                if(version == 3) {
                    if(m_server.more()) {
                        BOOST_VERIFY(m_server.recv(&signature));
                    }

                    if(!username.empty()) {
                        m_auth.verify(
                            static_cast<const char*>(message.data()),
                            message.size(),
                            static_cast<const unsigned char*>(signature.data()),
                            signature.size(),
                            username
                        );
                    } else {
                        throw std::runtime_error("username expected");
                    }
                }

                (void)respond(route, dispatch(root));
            } catch(const std::runtime_error& e) {
                (void)respond(route, helpers::make_json("error", e.what()));
            }
        } else {
            (void)respond(route, helpers::make_json("error", reader.getFormatedErrorMessages()));
        }
    } else {
        m_processor.stop();
    }
}

void core_t::pump(ev::timer&, int) {
    request(m_watcher, ev::READ);
}

Json::Value core_t::dispatch(const Json::Value& root) {
    std::string action(root["action"].asString());

    if(action == "create") {
        Json::Value apps(root["apps"]), result(Json::objectValue);

        if(!apps.isObject() || !apps.size()) {
            throw std::runtime_error("no apps have been specified");
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
            } catch(const std::exception& e) {
                result[app]["error"] = e.what();
            }
        }

        return result;
    } else if(action == "reload" || action == "delete") {
        Json::Value apps(root["apps"]), result(Json::objectValue);

        if(!apps.isArray() || !apps.size()) {
            throw std::runtime_error("no apps have been specified");
        }

        for(Json::Value::iterator it = apps.begin(); it != apps.end(); ++it) {
            std::string app((*it).asString());
            
            try {
                if(action == "reload") {
                    result[app] = reload_engine(app);
                } else {
                    result[app] = delete_engine(app);
                }
            } catch(const std::exception& e) {
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

Json::Value core_t::create_engine(const std::string& name, const Json::Value& manifest, bool recovering) {
    if(m_engines.find(name) != m_engines.end()) {
        throw std::runtime_error("the specified app is already active");
    }

    // Launch the engine
    std::auto_ptr<engine_t> engine(new engine_t(m_context, name));
    Json::Value result(engine->start(manifest));

    if(!recovering) {
        try {
            storage_t::create(m_context)->put("apps", name, manifest);
        } catch(const std::runtime_error& e) {
            syslog(LOG_ERR, "core: unable to create '%s' engine due to the storage failure - %s",
                name.c_str(), e.what());
            engine->stop();
            throw;
        }
    }

    // Only leave the engine running if all of the above succeded
    m_engines.insert(name, engine);
    
    return result;
}

Json::Value core_t::reload_engine(const std::string& name) {
    Json::Value manifest;
    engine_map_t::iterator engine(m_engines.find(name));

    if(engine == m_engines.end()) {
        throw std::runtime_error("the specified app is not active");
    }

    try {
        manifest = storage_t::create(m_context)->get("apps", name);
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "core: unable to reload '%s' engine due to the storage failure - %s",
            name.c_str(), e.what());
        throw;
    }

    engine->second->stop();
    m_engines.erase(engine);

    return create_engine(name, manifest, true);
}

Json::Value core_t::delete_engine(const std::string& name) {
    engine_map_t::iterator engine(m_engines.find(name));

    if(engine == m_engines.end()) {
        throw std::runtime_error("the specified app is not active");
    }

    try {
        storage_t::create(m_context)->remove("apps", name);
    } catch(const std::runtime_error& e) {
        syslog(LOG_ERR, "core: unable to destroy '%s' engine due to the storage failure - %s",
            name.c_str(), e.what());
        throw;
    }

    Json::Value result(engine->second->stop());
    m_engines.erase(engine);

    return result;
}

Json::Value core_t::info() const {
    Json::Value result(Json::objectValue);

    result["route"] = m_server.route();

    for(engine_map_t::const_iterator it = m_engines.begin(); it != m_engines.end(); ++it) {
        result["apps"][it->first] = it->second->info();
    }

    result["jobs"]["pending"] = static_cast<Json::UInt>(job::job_t::objects_alive);
    result["jobs"]["processed"] = static_cast<Json::UInt>(job::job_t::objects_created);

    result["uptime"] = ev::get_default_loop().now() - m_birthstamp;

    return result;
}

bool core_t::respond(const route_t& route, const Json::Value& object) {
    zmq::message_t message;
    
    // Send the identity
    for(route_t::const_iterator id = route.begin(); id != route.end(); ++id) {
        message.rebuild(id->size());
        memcpy(message.data(), id->data(), id->size());
        
        if(!m_server.send(message, ZMQ_SNDMORE)) {
            return false;
        }
    }

    // Send the delimiter
    message.rebuild(0);

    if(!m_server.send(message, ZMQ_SNDMORE)) {
        return false;
    }

    // Serialize and send the response
    std::string json(Json::FastWriter().write(object));
    message.rebuild(json.size());
    memcpy(message.data(), json.data(), json.size());

    return m_server.send(message);
}

void core_t::recover() {
    // NOTE: Allowing the exception to propagate here, as this is a fatal error.
    Json::Value root(storage_t::create(m_context)->all("apps"));

    if(root.size()) {
        Json::Value::Members apps(root.getMemberNames());
        
        syslog(LOG_NOTICE, "core: recovering %d %s: %s", root.size(),
            root.size() == 1 ? "app" : "apps", boost::algorithm::join(apps, ", ").c_str());
        
        for(Json::Value::Members::iterator it = apps.begin(); it != apps.end(); ++it) {
            std::string app(*it);
            
            try {
                create_engine(app, root[app], true);
            } catch(const std::exception& e) {
                throw std::runtime_error("unable to recover '" + app + "' app - " + e.what());
            }
        }
    }
}

void core_t::announce(ev::timer&, int) {
    syslog(LOG_DEBUG, "core: announcing the node");

    std::ostringstream envelope;

    envelope << m_context.config.core.instance << " "
             << m_server.endpoint();

    zmq::message_t message(envelope.str().size());

    memcpy(message.data(), envelope.str().data(), envelope.str().size());
    
    if(m_announces->send(message, ZMQ_SNDMORE)) {
        std::string announce(Json::FastWriter().write(info()));
        message.rebuild(announce.size());
        memcpy(message.data(), announce.data(), announce.size());
        
        (void)m_announces->send(message);
    }
}
