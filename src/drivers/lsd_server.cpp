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

#include "cocaine/drivers/lsd_server.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;
using namespace cocaine::networking;

lsd_job_t::lsd_job_t(lsd_server_t* driver, job::policy_t policy, const unique_id_t::type& id, const route_t& route):
    unique_id_t(id),
    job::job_t(driver, policy),
    m_route(route)
{ }

void lsd_job_t::react(const events::chunk_t& event) {
    zeromq_server_t* server = static_cast<zeromq_server_t*>(m_driver);
    Json::Value root(Json::objectValue);
    
    root["uuid"] = id();
    
    if(!send(root, ZMQ_SNDMORE) || !server->socket().send(event.message)) {
        syslog(LOG_ERR, "%s: unable to send the response", m_driver->identity());
    }
}

void lsd_job_t::react(const events::error_t& event) {
    Json::Value root(Json::objectValue);

    root["uuid"] = id();
    root["code"] = event.code;
    root["message"] = event.message;

    if(!send(root)) {
        syslog(LOG_ERR, "%s: unable to send the response", m_driver->identity());
    }
}

void lsd_job_t::react(const events::choked_t& event) {
    Json::Value root(Json::objectValue);

    root["uuid"] = id();
    root["completed"] = true;

    if(!send(root)) {
        syslog(LOG_ERR, "%s: unable to send the response", m_driver->identity());
    }
}

bool lsd_job_t::send(const Json::Value& root, int flags) {
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

    // Send the envelope
    std::string envelope(Json::FastWriter().write(root));
    message.rebuild(envelope.size());
    memcpy(message.data(), envelope.data(), envelope.size());

    return server->socket().send(message, flags);
}

lsd_server_t::lsd_server_t(engine_t* engine, const std::string& method, const Json::Value& args):
    zeromq_server_t(engine, method, args)
{ }

Json::Value lsd_server_t::info() const {
    Json::Value result(zeromq_server_t::info());

    result["type"] = "server+lsd";

    return result;
}

void lsd_server_t::process(ev::idle&, int) {
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
            syslog(LOG_ERR, "%s: got a corrupted request - no route", identity());
            return;
        }

        while(m_socket.more()) {
            // Receive the envelope
            BOOST_VERIFY(m_socket.recv(&message));

            // Parse the envelope and setup the job policy
            Json::Reader reader(Json::Features::strictMode());
            Json::Value root;

            if(!reader.parse(
                static_cast<const char*>(message.data()),
                static_cast<const char*>(message.data()) + message.size(),
                root))
            {
                syslog(LOG_ERR, "%s: got a corrupted request from '%s' - invalid envelope - %s",
                    identity(), route.back().c_str(), reader.getFormatedErrorMessages().c_str());
                continue;
            }

            job::policy_t policy(
                root.get("urgent", false).asBool(),
                root.get("timeout", 0.0f).asDouble(),
                root.get("deadline", 0.0f).asDouble());

            boost::shared_ptr<lsd_job_t> job;
            
            try {
                job.reset(new lsd_job_t(this, policy, root.get("uuid", "").asString(), route));
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "%s: got a corrupted request from '%s' - invalid envelope - %s",
                    identity(), route.back().c_str(), e.what());
                continue;
            }

            if(!m_socket.more() || !m_socket.recv(job->request())) {
                syslog(LOG_ERR, "%s: got a corrupted request from '%s' - missing body",
                    identity(), route.back().c_str());
                job->process_event(events::error_t(events::request_error, "missing body"));
                continue;
            }
            
            m_engine->enqueue(job);
        }
    } else {
        m_processor.stop();
        m_incoming = false;
    }
}

