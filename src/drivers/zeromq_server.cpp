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

#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>

#include "cocaine/drivers/zeromq_server.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;
using namespace cocaine::networking;

zeromq_server_job_t::zeromq_server_job_t(zeromq_server_t* driver, const route_t& route):
    job_t(driver, client::policy_t()),
    m_route(route)
{ }

void zeromq_server_job_t::react(const events::chunk_t& event) {
    if(!send(event.message)) {
        syslog(LOG_ERR, "%s: unable to send the response", m_driver->identity());
    }
}

void zeromq_server_job_t::react(const events::error_t& event) {
    Json::Value object(Json::objectValue);
    
    object["code"] = event.code;
    object["message"] = event.message;

    std::string response(Json::FastWriter().write(object));
    zmq::message_t message(response.size());
    memcpy(message.data(), response.data(), response.size());

    if(!send(message)) {
        syslog(LOG_ERR, "%s: unable to send the response", m_driver->identity());
    }
}

bool zeromq_server_job_t::send(zmq::message_t& chunk) {
    zmq::message_t message;
    zeromq_server_t* server = static_cast<zeromq_server_t*>(m_driver);
    
    // Send the identity
    for(route_t::const_iterator id = m_route.begin(); id != m_route.end(); ++id) {
        message.rebuild(id->size());
        memcpy(message.data(), id->data(), id->size());
        
        if(!server->socket().send(message, ZMQ_SNDMORE)) {
            return false;
        }
    }

    // Send the delimiter
    message.rebuild(0);

    if(!server->socket().send(message, ZMQ_SNDMORE)) {
        return false;
    }

    // Send the chunk
    return server->socket().send(chunk);
}

zeromq_server_t::zeromq_server_t(engine_t* engine, const std::string& method, const Json::Value& args, int type) try:
    driver_t(engine, method),
    m_backlog(args.get("backlog", 1000).asUInt()),
    m_linger(args.get("linger", 0).asInt()),
    m_socket(m_engine->context(), type, boost::algorithm::join(
        boost::assign::list_of
            (config_t::get().core.instance)
            (config_t::get().core.hostname)
            (m_engine->name())
            (method),
        "/")
    )
{
    std::string endpoint(args.get("endpoint", "").asString());

    if(endpoint.empty()) {
        throw std::runtime_error("no endpoint has been specified for the '" + m_method + "' task");
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
    throw std::runtime_error("network failure in '" + m_method + "' task - " + e.what());
}

zeromq_server_t::~zeromq_server_t() {
    m_watcher.stop();
    m_processor.stop();
    m_pumper.stop();
}

Json::Value zeromq_server_t::info() const {
    Json::Value result(Json::objectValue);

    result["statistics"] = stats();
    result["type"] = "zeromq-server";
    result["backlog"] = static_cast<Json::UInt>(m_backlog);
    result["endpoint"] = m_socket.endpoint();
    result["route"] = m_socket.route();

    return result;
}

void zeromq_server_t::event(ev::io&, int) {
    if(m_socket.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void zeromq_server_t::process(ev::idle&, int) {
    if(m_socket.pending()) {
        zmq::message_t message;
        route_t route;

        do {
            BOOST_VERIFY(m_socket.recv(&message));

            if(!message.size()) {
                break;
            }

            route.push_back(std::string(
                static_cast<const char*>(message.data()),
                message.size()));
        } while(m_socket.more());

        if(route.empty() || !m_socket.more()) {
            syslog(LOG_ERR, "%s: got a corrupted request - invalid route", identity());
            return;
        }

        while(m_socket.more()) {
            boost::shared_ptr<zeromq_server_job_t> job(new zeromq_server_job_t(this, route));

            BOOST_VERIFY(m_socket.recv(job->request()));
            m_engine->enqueue(job);
        }
    } else {
        m_processor.stop();
    }
}

void zeromq_server_t::pump(ev::timer&, int) {
    event(m_watcher, ev::READ);
}

