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

#include "cocaine/client.hpp"
#include "cocaine/drivers/native_sink.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;
using namespace cocaine::networking;

native_sink_t::native_sink_t(engine_t* engine, const std::string& method, const Json::Value& args):
    zeromq_sink_t(engine, method, args)
{ }

Json::Value native_sink_t::info() const {
    Json::Value result(zeromq_sink_t::info());

    result["type"] = "native-sink";

    return result;
}

void native_sink_t::process(ev::idle&, int) {
    if(m_socket.pending()) {
        do {
            unsigned int type = 0;
            client::request_t request;

            boost::tuple<unsigned int&, client::request_t&> tier(type, request);

            if(!m_socket.recv_multi(tier)) {
                syslog(LOG_ERR, "%s: got a corrupted request", identity());
                continue;
            }

            // TEST: This is temporary for testing purposes
            BOOST_ASSERT(type == request.type);

            boost::shared_ptr<publication_t> job(new publication_t(this, request.policy));
            
            if(!m_socket.more() || !m_socket.recv(job->request())) {
                syslog(LOG_ERR, "%s: got a corrupted request - missing body", identity());
                job->process_event(events::error_t(events::request_error, "missing body"));
                continue;
            }

            m_engine->enqueue(job);
        } while(m_socket.more());
    } else {
        m_processor.stop();
    }
}

