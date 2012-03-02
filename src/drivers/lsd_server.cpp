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

#include "cocaine/dealer/types.hpp"

using namespace cocaine::engine::drivers;
using namespace cocaine::networking;

lsd_job_t::lsd_job_t(lsd_server_t& driver, const client::policy_t& policy, const unique_id_t::type& id, const route_t& route):
    unique_id_t(id),
    job_t(driver, policy),
    m_route(route)
{ }

void lsd_job_t::react(const events::push_t& event) {
    Json::Value root(Json::objectValue);
    zeromq_server_t& server = static_cast<zeromq_server_t&>(m_driver);
    
    root["uuid"] = id();
    
    send(root, ZMQ_SNDMORE);
    server.socket().send(event.message);
}

void lsd_job_t::react(const events::error_t& event) {
    Json::Value root(Json::objectValue);

    root["uuid"] = id();
    root["code"] = event.code;
    root["message"] = event.message;

    send(root);
}

void lsd_job_t::react(const events::release_t& event) {
    Json::Value root(Json::objectValue);

    root["uuid"] = id();
    root["completed"] = true;

    send(root);
}

void lsd_job_t::send(const Json::Value& root, int flags) {
    zmq::message_t message;
    zeromq_server_t& server = static_cast<zeromq_server_t&>(m_driver);

    // Send the identity.
    for(route_t::const_iterator id = m_route.begin(); id != m_route.end(); ++id) {
        message.rebuild(id->size());
        memcpy(message.data(), id->data(), id->size());
        server.socket().send(message, ZMQ_SNDMORE);
    }

    // Send the delimiter.
    message.rebuild(0);
    server.socket().send(message, ZMQ_SNDMORE);

    // Send the envelope.
    std::string envelope(Json::FastWriter().write(root));
    
    message.rebuild(envelope.size());
    memcpy(message.data(), envelope.data(), envelope.size());
    server.socket().send(message, flags);
}

lsd_server_t::lsd_server_t(engine_t& engine, const std::string& method, const Json::Value& args):
    zeromq_server_t(engine, method, args, ZMQ_ROUTER)
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
            log().error("got a corrupted request - invalid route"); 
            return;
        }

        while(m_socket.more()) {
            // Receive the envelope.
            m_socket.recv(&message);

            // Parse the envelope and setup the job policy.
            Json::Reader reader(Json::Features::strictMode());
            Json::Value root;

            if(!reader.parse(
                static_cast<const char*>(message.data()),
                static_cast<const char*>(message.data()) + message.size(),
                root))
            {
                log().error(
                    "got a corrupted request - invalid envelope - %s",
                    reader.getFormatedErrorMessages().c_str()
                );

                continue;
            }

            client::policy_t policy(
                root.get("urgent", false).asBool(),
                root.get("timeout", 0.0f).asDouble(),
                root.get("deadline", 0.0f).asDouble()
            );

            boost::shared_ptr<lsd_job_t> job;
            
            try {
                job.reset(new lsd_job_t(*this, policy, root.get("uuid", "").asString(), route));
            } catch(const std::runtime_error& e) {
                log().error(
                    "got a corrupted request - invalid envelope - %s",
                    e.what()
                );

                continue;
            }

            if(!m_socket.more()) {
                log().error("got a corrupted request - missing body");
                job->process_event(events::error_t(client::request_error, "missing body"));
                continue;
            }
            
            m_socket.recv(&job->request());
            m_engine.enqueue(job);
        }
    } else {
        m_processor.stop();
    }
}
