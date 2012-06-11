/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/drivers/zeromq_sink.hpp"

#include "cocaine/context.hpp"
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
    int counter = m_engine.context().config.defaults.io_bulk_size;

    do {
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
    } while(--counter);
}

