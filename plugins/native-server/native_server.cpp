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
#include <boost/assign.hpp>

#include "native_server.hpp"
#include "native_job.hpp"

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"

using namespace cocaine;
using namespace cocaine::engine::drivers;
using namespace cocaine::io;

namespace msgpack {
    inline engine::policy_t& operator >> (msgpack::object o,
                                          engine::policy_t& object)
    {
        if(o.type != type::ARRAY || o.via.array.size != 3) {
            throw type_error();
        }

        msgpack::object &urgent = o.via.array.ptr[0],
                        &timeout = o.via.array.ptr[1],
                        &deadline = o.via.array.ptr[2];

        urgent >> object.urgent;
        timeout >> object.timeout;
        deadline >> object.deadline;

        return object;
    }
}

native_server_t::native_server_t(context_t& context, engine_t& engine, const plugin_config_t& config):
    category_type(context, engine, config),
    m_context(context),
    m_log(context.log("app/" + engine.manifest().name)),
    m_event(config.args["emit"].asString()),
    m_route(boost::algorithm::join(
        boost::assign::list_of
            (context.config.runtime.hostname)
            (engine.manifest().name)
            (config.name),
        "/")
    ),
    m_watcher(engine.loop()),
    m_processor(engine.loop()),
    // m_pumper(engine.loop()),
    m_check(engine.loop()),
    m_channel(context.io(), ZMQ_ROUTER, m_route)
{
    int linger = 0;

    try {
        m_channel.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
        m_channel.bind(config.args["endpoint"].asString());
    } catch(const zmq::error_t& e) {
        throw configuration_error_t(std::string("invalid driver endpoint - ") + e.what());
    }

    m_watcher.set<native_server_t, &native_server_t::event>(this);
    m_watcher.start(m_channel.fd(), ev::READ);
    m_processor.set<native_server_t, &native_server_t::process>(this);
    m_check.set<native_server_t, &native_server_t::check>(this);
    m_check.start();
    // m_pumper.set<native_server_t, &native_server_t::pump>(this);
    // m_pumper.start(0.005f, 0.005f);
}

native_server_t::~native_server_t() {
    m_watcher.stop();
    m_processor.stop();
    m_check.stop();
    // m_pumper.stop();
}

Json::Value native_server_t::info() const {
    Json::Value result;

    result["endpoint"] = m_channel.endpoint();
    result["route"] = m_route;
    result["type"] = "native-server";

    return result;
}

void native_server_t::process(ev::idle&, int) {
    int counter = defaults::io_bulk_size;
    
    do {
        if(!m_channel.pending()) {
            m_processor.stop();
            return;
        }
       
        zmq::message_t message;
        route_t route;

        do {
            m_channel.recv(&message);

            if(!message.size()) {
                break;
            }

            route.push_back(
                std::string(
                    static_cast<const char*>(message.data()),
                    message.size()
                )
            );
        } while(m_channel.more());

        if(route.empty() || !m_channel.more()) {
            m_log->error(
                "received a corrupted request",
                m_event.c_str()
            );

            m_channel.drop();
            return;
        }

        m_log->debug("received a request from '%s'", route[0].c_str());

        do {
            std::string tag;
            engine::policy_t policy;

            request_proxy_t proxy(tag, policy, &message);

            try {
                m_channel.recv_multi(proxy);
            } catch(const std::runtime_error& e) {
                m_log->error(
                    "received a corrupted request - %s",
                    m_event.c_str(),
                    e.what()
                );
        
                m_channel.drop();
                return;
            }

            m_log->debug(
                "enqueuing a '%s' job with uuid: %s",
                m_event.c_str(),
                tag.c_str()
            );
            
            engine().enqueue(
                boost::make_shared<native_job_t>(
                    m_event,
                    blob_t(
                        message.data(), 
                        message.size()
                    ),
                    policy,
                    boost::ref(m_channel),
                    route,
                    tag
                )
            );
        } while(m_channel.more());
    } while(--counter);
}

void native_server_t::event(ev::io&, int) {
    if(m_channel.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void native_server_t::check(ev::prepare&, int) {
    event(m_watcher, ev::READ);
}

// void native_server_t::pump(ev::timer&, int) {
//     event(m_watcher, ev::READ);
// }

extern "C" {
    void initialize(repository_t& repository) {
        repository.insert<native_server_t>("native-server");
    }
}
