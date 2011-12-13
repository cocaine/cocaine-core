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

#include <boost/algorithm/string/join.hpp>
#include <boost/assign.hpp>

#include "cocaine/drivers/zeromq_sink.hpp"
#include "cocaine/engine.hpp"

using namespace cocaine::engine::driver;
using namespace cocaine::networking;

zeromq_sink_t::zeromq_sink_t(engine_t* engine, const std::string& method, const Json::Value& args) try:
    driver_t(engine, method),
    m_backlog(args.get("backlog", 1000).asUInt()),
    m_socket(m_engine->context(), ZMQ_PULL, boost::algorithm::join(
        boost::assign::list_of
            (config_t::get().core.instance)
            (config_t::get().core.hostname)
            (m_engine->name())
            (method),
        "/")
    )
{
    std::string endpoint(args.get("endpoint", "").asString());

    if(endpoint.empty()) {
        throw std::runtime_error("no endpoint has been specified for the '" + m_method + "' task");
    }

    m_socket.setsockopt(ZMQ_HWM, &m_backlog, sizeof(m_backlog));
    m_socket.bind(endpoint);

    m_watcher.set<zeromq_sink_t, &zeromq_sink_t::event>(this);
    m_watcher.start(m_socket.fd(), ev::READ);
    m_processor.set<zeromq_sink_t, &zeromq_sink_t::process>(this);
    m_pumper.set<zeromq_sink_t, &zeromq_sink_t::pump>(this);
    m_pumper.start(0.2f, 0.2f);
} catch(const zmq::error_t& e) {
    throw std::runtime_error("network failure in '" + m_method + "' task - " + e.what());
}

zeromq_sink_t::~zeromq_sink_t() {
    m_watcher.stop();
    m_processor.stop();
    m_pumper.stop();
}

Json::Value zeromq_sink_t::info() const {
    Json::Value result(Json::objectValue);

    result["statistics"] = stats();
    result["type"] = "zeromq-sink";
    result["backlog"] = static_cast<Json::UInt>(m_backlog);
    result["endpoint"] = m_socket.endpoint();
    result["route"] = m_socket.route();

    return result;
}

void zeromq_sink_t::event(ev::io&, int) {
    if(m_socket.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void zeromq_sink_t::process(ev::idle&, int) {
    if(m_socket.pending()) {
        do {
            boost::shared_ptr<publication_t> job(new publication_t(this));
            BOOST_VERIFY(m_socket.recv(job->request()));
            m_engine->enqueue(job);
        } while(m_socket.more()); 
    } else {
        m_processor.stop();
    }
}

void zeromq_sink_t::pump(ev::timer&, int) {
    event(m_watcher, ev::READ);
}

