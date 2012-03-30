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

#include "cocaine/drivers/native_server.hpp"

#include "cocaine/engine.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/rpc.hpp"

using namespace cocaine::engine::drivers;
using namespace cocaine::networking;

native_job_t::native_job_t(native_server_t& driver, 
                           const client::policy_t& policy, 
                           const blob_t& request, 
                           const route_t& route,
                           const std::string& tag):
    job_t(driver, policy, request),
    m_route(route),
    m_tag(tag)
{ }

void native_job_t::react(const events::push_t& event) {
    rpc::packed<rpc::push> pack(event.message);
    send(pack);
}

void native_job_t::react(const events::error_t& event) {
    rpc::packed<rpc::error> pack(event.code, event.message);
    send(pack);
}

void native_job_t::react(const events::release_t& event) {
    rpc::packed<rpc::release> pack;
    send(pack);
}

native_server_t::native_server_t(engine_t& engine, const std::string& method, const Json::Value& args):
    zeromq_server_t(engine, method, args, ZMQ_ROUTER)
{ }

Json::Value native_server_t::info() {
    Json::Value result(zeromq_server_t::info());

    result["type"] = "native-server";

    return result;
}

void native_server_t::process(ev::idle&, int) {
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

        m_socket.drop_remaining_parts();
        return;
    }

    while(m_socket.more()) {
        std::string tag;
        client::policy_t policy;

        request_proxy_t tier(tag, policy, &message);

        try {
            if(!m_socket.recv_multi(tier, ZMQ_NOBLOCK)) {
                throw std::runtime_error("incomplete object");
            }
        } catch(const std::runtime_error& e) {
            m_engine.app().log->error(
                "driver %s got a corrupted request - %s",
                m_method.c_str(),
                e.what()
            );
    
            m_socket.drop_remaining_parts();
            return;
        }

        m_engine.enqueue(
            new native_job_t(
                *this,
                policy,
                blob_t(
                    message.data(), 
                    message.size()
                ),
                route,
                tag
            )
        );
    }
}
