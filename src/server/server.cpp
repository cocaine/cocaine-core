/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>

#include "cocaine/server/server.hpp"

#include "cocaine/app.hpp"
#include "cocaine/context.hpp"
#include "cocaine/job.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/api/storage.hpp"

using namespace cocaine;
using namespace cocaine::helpers;

server_t::server_t(context_t& context, server_config_t config):
    m_context(context),
    m_log(context.log("core")),
    m_runlist(config.runlist),
    m_server(context, ZMQ_REP, context.config.runtime.hostname),
    m_auth(context),
    m_birthstamp(m_loop.now()) /* I don't like it. */,
    m_infostamp(0.0f)
{
    int minor, major, patch;
    zmq_version(&major, &minor, &patch);

    m_log->info("using libev version %d.%d", ev_version_major(), ev_version_minor());
    m_log->info("using libmsgpack version %s", msgpack_version());
    m_log->info("using libzmq version %d.%d.%d", major, minor, patch);
    m_log->info("route to this node is '%s'", m_server.route().c_str());

    // Server socket
    // -------------

    int linger = 0;

    m_server.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    for(std::vector<std::string>::const_iterator it = config.listen_endpoints.begin();
        it != config.listen_endpoints.end();
        ++it)
    {
        try {
            m_server.bind(*it);
        } catch(const zmq::error_t& e) {
            boost::format message("invalid listen endpoint - %s");
            throw configuration_error_t((message % e.what()).str());
        }
            
        m_log->info("listening on %s", it->c_str());
    }
    
    m_watcher.set<server_t, &server_t::event>(this);
    m_watcher.start(m_server.fd(), ev::READ);
    m_checker.set<server_t, &server_t::check>(this);
    m_checker.start();

    // Autodiscovery
    // -------------

    if(!config.announce_endpoints.empty()) {
        m_announces.reset(new io::socket_t(m_context, ZMQ_PUB));
        m_announces->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
        
        for(std::vector<std::string>::const_iterator it = config.announce_endpoints.begin();
            it != config.announce_endpoints.end();
            ++it)
        {
            try {
                m_announces->connect(*it);
            } catch(const zmq::error_t& e) {
                boost::format message("invalid announce endpoint - %s");
                throw configuration_error_t((message % e.what()).str());
            }

            m_log->info("announcing on %s", it->c_str());
        }

        m_announce_timer.reset(new ev::timer());
        m_announce_timer->set<server_t, &server_t::announce>(this);
        m_announce_timer->start(0.0f, config.announce_interval);
    }

    // Signals
    // -------

    m_sigint.set<server_t, &server_t::terminate>(this);
    m_sigint.start(SIGINT);

    m_sigterm.set<server_t, &server_t::terminate>(this);
    m_sigterm.start(SIGTERM);

    m_sigquit.set<server_t, &server_t::terminate>(this);
    m_sigquit.start(SIGQUIT);

    m_sighup.set<server_t, &server_t::reload>(this);
    m_sighup.start(SIGHUP);
   
    recover();
}

server_t::~server_t() { }

void server_t::run() {
    m_loop.loop();
}

void server_t::terminate(ev::sig&, int) {
    if(!m_apps.empty()) {
        m_log->info("stopping the apps");
        m_apps.clear();
    }

    m_loop.unloop(ev::ALL);
}

void server_t::reload(ev::sig&, int) {
    m_log->info("reloading the apps");

    try {
        recover();
    } catch(const std::exception& e) {
        m_log->error("unable to reload the apps - %s", e.what());
    } catch(...) {
        m_log->error("unable to reload the apps - unexpected exception");
    }
}

void server_t::event(ev::io&, int) {
    m_checker.stop();

    if(m_server.pending()) {
        m_checker.start();
        process();
    }
}

void server_t::check(ev::prepare&, int) {
    m_loop.feed_fd_event(m_server.fd(), ev::READ);
}

void server_t::process() {
    zmq::message_t message;
    
    {
        io::scoped_option<
            io::options::receive_timeout
        > option(m_server, 0);
        
        if(!m_server.recv(&message)) {
            return;
        }
    }

    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;
    std::string response;

    if(reader.parse(
        static_cast<const char*>(message.data()),
        static_cast<const char*>(message.data()) + message.size(),
        root)) 
    {
        try {
            if(!root.isObject()) {
                throw configuration_error_t("json root must be an object");
            }

            unsigned int version = root["version"].asUInt();
            
            if(version < 2 || version > 3) {
                throw configuration_error_t("unsupported protocol version");
            }
  
            if(version == 3) {
                zmq::message_t signature;

                if(m_server.more()) {
                    m_server.recv(&signature);
                }

                std::string username(root["username"].asString());
                
                if(!username.empty()) {
                    m_auth.verify(
                        std::string(static_cast<const char*>(message.data()), message.size()),
                        std::string(static_cast<const char*>(signature.data()), signature.size()),
                        username
                    );
                } else {
                    throw authorization_error_t("username expected");
                }
            }

            response = dispatch(root);
        } catch(const std::exception& e) {
            response = json::serialize(json::build("error", e.what()));
        } catch(...) {
            response = json::serialize(json::build("error", "unexpected exception"));
        }
    } else {
        response = json::serialize(
            json::build("error", reader.getFormattedErrorMessages())
        );
    }

    // Serialize and send the response.
    message.rebuild(response.size());
    memcpy(message.data(), response.data(), response.size());

    // NOTE: Do a non-blocking send.
    io::scoped_option<
        io::options::send_timeout
    > option(m_server, 0);
    
    m_server.send(message);
}

std::string server_t::dispatch(const Json::Value& root) {
    std::string action(root["action"].asString());

    if(action == "create") {
        Json::Value apps(root["apps"]),
                    result(Json::objectValue);

        if(!apps.isObject() || apps.empty()) {
            throw configuration_error_t("no apps have been specified");
        }

        Json::Value::Members names(apps.getMemberNames());

        for(Json::Value::Members::const_iterator it = names.begin(); it != names.end(); ++it) {
            std::string name(*it),
                        profile(apps[name].asString());

            try {
                result[name] = create_app(name, profile);
            } catch(const std::exception& e) {
                result[name]["error"] = e.what();
            } catch(...) {
                result[name]["error"] = "unexpected exception";
            }
        }

        return json::serialize(result);
    } else if(action == "delete") {
        Json::Value apps(root["apps"]),
                    result(Json::objectValue);

        if(!apps.isArray() || apps.empty()) {
            throw configuration_error_t("no apps have been specified");
        }

        for(Json::Value::iterator it = apps.begin(); it != apps.end(); ++it) {
            std::string name((*it).asString());

            try {
                result[name] = delete_app(name);                
            } catch(const std::exception& e) {
                result[name]["error"] = e.what();
            } catch(...) {
                result[name]["error"] = "unexpected exception";
            }
        }

        return json::serialize(result);
    } else if(action == "info") {
        if(m_loop.now() >= (m_infostamp + 5.0f)) {
            m_infostamp = m_loop.now();
            m_infocache = json::serialize(info());
        }

        return m_infocache;
    } else {
        throw configuration_error_t("unsupported action");
    }
}

// Commands
// --------

Json::Value server_t::create_app(const std::string& name, const std::string& profile) {
    if(m_apps.find(name) != m_apps.end()) {
        throw configuration_error_t("the specified app already exists");
    }

    // DEPRECATED: In order to support boost::ptr_map.
    std::auto_ptr<app_t> app(
        new app_t(
            m_context,
            name,
            profile
        )
    );

    app->start();

    Json::Value result(app->info());

    m_apps.insert(name, app);
    
    return result;
}

Json::Value server_t::delete_app(const std::string& name) {
    app_map_t::iterator app(m_apps.find(name));

    if(app == m_apps.end()) {
        throw configuration_error_t("the specified app doesn't exist");
    }

    app->second->stop();

    Json::Value result(app->second->info());

    m_apps.erase(app);

    return result;
}

Json::Value server_t::info() const {
    Json::Value result(Json::objectValue);

    result["route"] = m_context.config.runtime.hostname;

    for(app_map_t::const_iterator it = m_apps.begin();
        it != m_apps.end(); 
        ++it) 
    {
        result["apps"][it->first] = it->second->info();
    }

    result["loggers"] = static_cast<Json::UInt>(logging::logger_t::objects_alive());
    result["sockets"] = static_cast<Json::UInt>(io::socket_t::objects_alive());

    result["jobs"]["pending"] = static_cast<Json::UInt>(engine::job_t::objects_alive());
    result["jobs"]["processed"] = static_cast<Json::UInt>(engine::job_t::objects_created());

    result["uptime"] = m_loop.now() - m_birthstamp;

    return result;
}

void server_t::announce(ev::timer&, int) {
    m_log->debug("announcing the node");

    zmq::message_t message(m_server.endpoint().size());
 
    memcpy(message.data(), m_server.endpoint().data(), m_server.endpoint().size());
    m_announces->send(message, ZMQ_SNDMORE);

    std::string announce(Json::FastWriter().write(info()));
    
    message.rebuild(announce.size());
    memcpy(message.data(), announce.data(), announce.size());
    m_announces->send(message);
}

void server_t::recover() {
    typedef std::map<
        std::string,
        std::string
    > runlist_t;

    api::category_traits<api::storage_t>::ptr_type storage(
        m_context.get<api::storage_t>("storage/core")
    );

    m_log->info("reading the '%s' runlist", m_runlist.c_str());
    
    // NOTE: Allowing the exception to propagate here, as this is a fatal error.
    runlist_t runlist(
        storage->get<runlist_t>("runlists", m_runlist)
    );

    std::set<std::string> active,
                          available;

    // Currently running apps.
    for(app_map_t::const_iterator it = m_apps.begin();
        it != m_apps.end();
        ++it)
    {
        active.insert(it->first);
    }

    // Runnable apps.
    for(runlist_t::const_iterator it = runlist.begin();
        it != runlist.end();
        ++it)
    {
        available.insert(it->first);
    }

    std::vector<std::string> diff;

    // Generate a list of apps which are either new or dead.
    std::set_symmetric_difference(active.begin(), active.end(),
                                  available.begin(), available.end(),
                                  std::back_inserter(diff));

    if(diff.size()) {
        for(std::vector<std::string>::const_iterator it = diff.begin();
            it != diff.end(); 
            ++it)
        {
            if(m_apps.find(*it) == m_apps.end()) {
                try {
                    create_app(*it, runlist[*it]);
                } catch(const std::exception& e) {
                    m_log->error(
                        "unable to initialize the '%s' app - %s",
                        it->c_str(),
                        e.what()
                    );

                    throw configuration_error_t("unable to initialize the apps");
                }
            } else {
                m_apps.find(*it)->second->stop();
            }
        }
    }
}

