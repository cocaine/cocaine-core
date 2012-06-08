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

#include "native_job.hpp"

#include "cocaine/rpc.hpp"

#include "cocaine/dealer/types.hpp"

using namespace cocaine::engine;
using namespace cocaine::engine::drivers;
using namespace cocaine::networking;

native_job_t::native_job_t(const std::string& event, 
                           const blob_t& request,
                           const policy_t& policy,
                           channel_t& channel,
                           const route_t& route,
                           const std::string& tag):
    job_t(event, request, policy),
    m_channel(channel),
    m_route(route),
    m_tag(tag)
{
    rpc::packed<dealer::acknowledgement> pack;
    send(pack);
}

void native_job_t::react(const events::chunk& event) {
    rpc::packed<dealer::chunk> pack(event.message);
    send(pack);
}

void native_job_t::react(const events::error& event) {
    rpc::packed<dealer::error> pack(event.code, event.message);
    send(pack);
}

void native_job_t::react(const events::choke& event) {
    rpc::packed<dealer::choke> pack;
    send(pack);
}
