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

#include "cocaine/drivers/base.hpp"

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::drivers;
        
driver_t::driver_t(engine_t& engine, const std::string& method, const Json::Value& args):
    object_t(engine.context()),
    m_engine(engine),
    m_method(method)
#if BOOST_VERSION < 103600
    , m_spent_in_queues(0.0f)
    , m_spent_on_slaves(0.0f)
#endif
{ }

driver_t::~driver_t() { }

void driver_t::audit(timing_type type, ev::tstamp value) {
    switch(type) {
        case in_queue:
#if BOOST_VERSION >= 103600
            m_spent_in_queues(value);
#else
            m_spent_in_queues += value;
#endif
            break;
            
        case on_slave:
#if BOOST_VERSION >= 103600
            m_spent_on_slaves(value);
#else
            m_spent_on_slaves += value;
#endif
            break;
    }
}

Json::Value driver_t::info() {
    Json::Value results(Json::objectValue);

#if BOOST_VERSION >= 103600
    results["stats"]["time-spent-on-slaves"] = sum(m_spent_on_slaves);
    results["stats"]["median-processing-time"] = median(m_spent_on_slaves);
    results["stats"]["time-spent-in-queues"] = sum(m_spent_in_queues);
    results["stats"]["median-wait-time"] = median(m_spent_in_queues);
#else
    results["stats"]["time-spent-on-slaves"] = m_spent_on_slaves;
    results["stats"]["time-spent-in-queues"] = m_spent_in_queues;
#endif

    return results;
}
