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

#include "cocaine/client/types.hpp"
#include "cocaine/drivers/native_server.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;
using namespace cocaine::networking;

native_server_job_t::native_server_job_t(const unique_id_t::type& id, native_server_t& driver, const client::policy_t& policy, const route_t& route):
    unique_id_t(id),
    job::job_t(driver, policy),
    m_route(route)
{ }

void native_server_job_t::react(const events::chunk_t& event) {
    zeromq_server_t& server = static_cast<zeromq_server_t&>(m_driver);
    
    if(!send(client::tag_t(id()), ZMQ_SNDMORE) || !server.socket().send(event.message)) {
        syslog(LOG_ERR, "%s: unable to send the response", m_driver.identity());
    }
}

void native_server_job_t::react(const events::error_t& event) {
    if(!send(client::error_t(id(), event.code, event.message))) {
        syslog(LOG_ERR, "%s: unable to send the response", m_driver.identity());
    }
}

void native_server_job_t::react(const events::choked_t& event) {
    if(!send(client::tag_t(id(), true))) {
        syslog(LOG_ERR, "%s: unable to send the response", m_driver.identity());
    }
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
            unsigned int type = 0;
            client::tag_t tag;
            client::policy_t policy;

            boost::tuple<unsigned int&, client::tag_t&, client::policy_t&> tier(type, tag, policy);

            if(!m_socket.recv_multi(tier)) {
                syslog(LOG_ERR, "%s: got a corrupted request", identity());
                continue;
            }

            // TEST: This is temporary for testing purposes
            BOOST_ASSERT(type == tag.type);
            boost::shared_ptr<native_server_job_t> job;
            
            try {
                job.reset(new native_server_job_t(tag.id, *this, policy, route));
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "%s: got a corrupted request - %s", identity(), e.what());
                continue;
            }

            if(!m_socket.more() || !m_socket.recv(job->request())) {
                syslog(LOG_ERR, "%s: got a corrupted request - missing body", identity());
                job->process_event(events::error_t(client::request_error, "missing body"));
                continue;
            }

            m_engine.enqueue(job);
        }
    } else {
        m_processor.stop();
    }
}

