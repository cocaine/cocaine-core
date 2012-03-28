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

#include "cocaine/drivers/zeromq_sink.hpp"

#include "cocaine/engine.hpp"

using namespace cocaine::engine::drivers;

zeromq_sink_t::zeromq_sink_t(engine_t& engine,
                             const std::string& method, 
                             const Json::Value& args):
    zeromq_server_t(engine, method, args, ZMQ_PULL)
{ }

Json::Value zeromq_sink_t::info() {
    Json::Value result(zeromq_server_t::info());

    result["type"] = "zeromq-sink";

    return result;
}

void zeromq_sink_t::process(ev::idle&, int) {
    if(!m_socket.pending()) {
        m_processor.stop();
        return;
    }
    
    zmq::message_t message;

    do {
        m_socket.recv(&message);

        m_engine.enqueue(
            new job_t(
                *this,
                blob_t(
                    message.data(), 
                    message.size()
                )
            )
        );
    } while(m_socket.more()); 
}
