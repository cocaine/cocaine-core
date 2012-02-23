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
#include "cocaine/drivers/native_sink.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;
using namespace cocaine::networking;

native_sink_t::native_sink_t(engine_t& engine, const std::string& method, const Json::Value& args):
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
            client::tag_t tag;
            client::policy_t policy;
            boost::tuple<unsigned int&, client::tag_t&, client::policy_t&> tier(type, tag, policy);

            m_socket.recv_multi(tier);

            // TEST: This is temporary for testing purposes
            BOOST_ASSERT(type == tag.type);
            boost::shared_ptr<job::job_t> job(new job::job_t(*this, policy));
            
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
        } while(m_socket.more());
    } else {
        m_processor.stop();
    }
}

