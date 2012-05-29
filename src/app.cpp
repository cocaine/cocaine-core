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

#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>

#include "cocaine/app.hpp"

#include "cocaine/context.hpp"

#include "cocaine/interfaces/storage.hpp"

using namespace cocaine::engine;
using namespace cocaine::storages;

// Application
// -----------

app_t::app_t(context_t& context, const std::string& name_):
    name(name_),
    log(context.log(name_))
{
    boost::shared_ptr<storage_t> storage(
        context.get<storage_t>(context.config.storage.driver)
    );

    manifest = storage->get("apps", name_);

    policy.heartbeat_timeout = manifest["engine"].get(
        "heartbeat-timeout",
        context.config.defaults.heartbeat_timeout
    ).asDouble();

    policy.suicide_timeout = manifest["engine"].get(
        "suicide-timeout",
        context.config.defaults.suicide_timeout
    ).asDouble();
    
    policy.pool_limit = manifest["engine"].get(
        "pool-limit",
        context.config.defaults.pool_limit
    ).asUInt();
    
    policy.queue_limit = manifest["engine"].get(
        "queue-limit",
        context.config.defaults.queue_limit
    ).asUInt();

    policy.grow_threshold = manifest["engine"].get(
        "grow-threshold",
        policy.queue_limit / policy.pool_limit
    ).asUInt();
}

endpoint::endpoint(const std::string& name) {
    m_endpoint = boost::algorithm::join(
        boost::assign::list_of
            (std::string("ipc:///var/run/cocaine"))
            (name),
        "/");
}

endpoint::operator std::string() const {
    return m_endpoint;
}
