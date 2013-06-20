/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "cocaine/detail/profile.hpp"
#include "cocaine/traits/json.hpp"

using namespace cocaine::engine;

profile_t::profile_t(context_t& context, const std::string& name_):
    cached<Json::Value>(context, "profiles", name_),
    name(name_)
{
    log_output = get(
        "log-output",
        defaults::log_output
    ).asBool();

    heartbeat_timeout = get(
        "heartbeat-timeout",
        defaults::heartbeat_timeout
    ).asDouble();

    if(heartbeat_timeout <= 0.0f) {
        throw configuration_error_t("slave heartbeat timeout must be positive");
    }

    idle_timeout = get(
        "idle-timeout",
        defaults::idle_timeout
    ).asDouble();

    if(idle_timeout < 0.0f) {
        throw configuration_error_t("slave idle timeout must non-negative");
    }

    startup_timeout = get(
        "startup-timeout",
        defaults::startup_timeout
    ).asDouble();

    if(startup_timeout <= 0.0f) {
        throw configuration_error_t("slave startup timeout must be positive");
    }

    termination_timeout = get(
        "termination-timeout",
        defaults::termination_timeout
    ).asDouble();

    if(termination_timeout <= 0.0f) {
        throw configuration_error_t("engine termination timeout must be non-negative");
    }

    concurrency = get(
        "concurrency",
        static_cast<Json::UInt>(defaults::concurrency)
    ).asUInt();

    crashlog_limit = get(
        "crashlog-limit",
        static_cast<Json::UInt>(defaults::crashlog_limit)
    ).asUInt();

    pool_limit = get(
        "pool-limit",
        static_cast<Json::UInt>(defaults::pool_limit)
    ).asUInt();

    if(pool_limit == 0) {
        throw configuration_error_t("engine pool limit must be positive");
    }

    queue_limit = get(
        "queue-limit",
        static_cast<Json::UInt>(defaults::queue_limit)
    ).asUInt();

    if(concurrency == 0) {
        throw configuration_error_t("engine concurrency must be positive");
    }

    grow_threshold = get(
        "grow-threshold",
        std::max(
            static_cast<Json::UInt>(1UL),
            static_cast<Json::UInt>(queue_limit / pool_limit * concurrency)
        )
    ).asUInt();

    // Isolation

    isolate = {
        (*this)["isolate"].get("type", "process").asString(),
        (*this)["isolate"]["args"]
    };
}

