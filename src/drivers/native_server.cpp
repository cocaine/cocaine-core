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

#include "cocaine/dealer/types.hpp"
#include "cocaine/drivers/native_server.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;
using namespace cocaine::networking;

native_server_job_t::native_server_job_t(native_server_t& driver, const client::policy_t& policy, const unique_id_t::type& id, const route_t& route):
    unique_id_t(id),
    job::job_t(driver, policy),
    m_route(route)
{ }

void native_server_job_t::react(const events::push_t& event) {
    zeromq_server_t& server = static_cast<zeromq_server_t&>(m_driver);
    
    send(client::tag_t(id()), ZMQ_SNDMORE);
    server.socket().send(event.message);
}

void native_server_job_t::react(const events::error_t& event) {
    job_t::react(event);
    send(client::error_t(id(), event.code, event.message));
}

void native_server_job_t::react(const events::release_t& event) {
    job_t::react(event);
    send(client::tag_t(id(), true));
}

native_server_t::native_server_t(engine_t& engine, const std::string& method, const Json::Value& args):
    zeromq_server_t(engine, method, args)
{ }

Json::Value native_server_t::info() const {
    Json::Value result(zeromq_server_t::info());

    result["type"] = "native-server";

    return result;
}

void native_server_t::process(ev::idle&, int) {
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
            m_engine.log().error(
                "got a corrupted request in '%s' - invalid route",
                m_method.c_str()
            );

            return;
        }

        while(m_socket.more()) {
            unsigned int type = 0;
            client::tag_t tag;
            client::policy_t policy;
            boost::tuple<unsigned int&, client::tag_t&, client::policy_t&> tier(type, tag, policy);

            m_socket.recv_multi(tier);

            // TEST: This is temporary for testing purposes
            BOOST_ASSERT(type == tag.type);
            boost::shared_ptr<native_server_job_t> job;
            
            try {
                job.reset(new native_server_job_t(*this, policy, tag.id, route));
            } catch(const std::runtime_error& e) {
                m_engine.log().error(
                    "got a corrupted request in '%s' - %s", 
                    m_method.c_str(), 
                    e.what()
                );

                continue;
            }

            if(!m_socket.more()) {
                m_engine.log().error(
                    "got a corrupted request in '%s' - missing body", 
                    m_method.c_str()
                );
                
                job->process_event(events::error_t(client::request_error, "missing body"));
                
                continue;
            }

            m_socket.recv(job->request());
            m_engine.enqueue(job);
        }
    } else {
        m_processor.stop();
    }
}

