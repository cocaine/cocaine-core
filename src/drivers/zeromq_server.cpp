//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
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

#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>

#include "cocaine/drivers/zeromq_server.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/engine.hpp"

using namespace cocaine::engine::drivers;
using namespace cocaine::networking;

zeromq_server_job_t::zeromq_server_job_t(
    zeromq_server_t& driver,
    const blob_t& request,
    const route_t& route
):
    job_t(driver, request),
    m_route(route)
{ }

void zeromq_server_job_t::react(const events::push_t& event) {
    zeromq_server_t& server = static_cast<zeromq_server_t&>(m_driver);

    try {    
        std::for_each(m_route.begin(), m_route.end(), route(server.socket()));
    } catch(const zmq::error_t& e) {
        // Host is down.
        return;
    }

    zmq::message_t message;

    server.socket().send(message, ZMQ_SNDMORE);
    server.socket().send(event.message);
}

zeromq_server_t::zeromq_server_t(engine_t& engine, const std::string& method, const Json::Value& args, int type) try:
    driver_t(engine, method, args),
    m_backlog(args.get("backlog", 1000).asUInt()),
    m_linger(args.get("linger", 0).asInt()),
    m_socket(m_engine.context(), type, boost::algorithm::join(
        boost::assign::list_of
            (m_engine.context().config.runtime.hostname)
            (m_engine.app().name)
            (method),
        "/")
    )
{
    std::string endpoint(args.get("endpoint", "").asString());

    if(endpoint.empty()) {
        throw std::runtime_error("no endpoint has been specified");
    }

    m_socket.setsockopt(ZMQ_HWM, &m_backlog, sizeof(m_backlog));
    m_socket.setsockopt(ZMQ_LINGER, &m_linger, sizeof(m_linger));

    m_socket.bind(endpoint);

    m_watcher.set<zeromq_server_t, &zeromq_server_t::event>(this);
    m_watcher.start(m_socket.fd(), ev::READ);
    m_processor.set<zeromq_server_t, &zeromq_server_t::process>(this);
    m_pumper.set<zeromq_server_t, &zeromq_server_t::pump>(this);
    m_pumper.start(0.2f, 0.2f);
} catch(const zmq::error_t& e) {
    throw std::runtime_error(std::string("network failure - ") + e.what());
}

zeromq_server_t::~zeromq_server_t() {
    m_watcher.stop();
    m_processor.stop();
    m_pumper.stop();
}

Json::Value zeromq_server_t::info() const {
    Json::Value result(driver_t::info());

    result["type"] = "zeromq-server";
    result["backlog"] = static_cast<Json::UInt>(m_backlog);
    result["endpoint"] = m_socket.endpoint();
    result["route"] = m_socket.route();

    return result;
}

void zeromq_server_t::process(ev::idle&, int) {
    if(!m_socket.pending()) {
        m_processor.stop();
        return;
    }
    
    zmq::message_t message;
    route_t route;

    do {
        m_socket.recv(&message);

        if(!message.size()) {
            break;
        }

        route.push_back(
            std::string(
                static_cast<const char*>(message.data()),
                message.size()
            )
        );
    } while(m_socket.more());

    if(route.empty() || !m_socket.more()) {
        m_engine.app().log->error(
            "driver '%s' got a corrupted request",
            m_method.c_str()
        );
        
        return;
    }

    while(m_socket.more()) {
        m_socket.recv(&message);

        m_engine.enqueue(
            boost::make_shared<zeromq_server_job_t>(
                boost::ref(*this),
                blob_t(
                    message.data(), 
                    message.size()
                ),
                route
            )
        );
    }
}

void zeromq_server_t::event(ev::io&, int) {
    if(m_socket.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void zeromq_server_t::pump(ev::timer&, int) {
    event(m_watcher, ev::READ);
}
