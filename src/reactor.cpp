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

#include "cocaine/reactor.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;

reactor_t::reactor_t(context_t& context,
                     const std::string& name,
                     const Json::Value& args):
    category_type(context, name, args),
    m_context(context),
    m_log(new logging::log_t(m_context, name)),
    m_channel(context, ZMQ_ROUTER),
    m_watcher(m_loop),
    m_checker(m_loop),
    m_terminate(m_loop)
{
    if(args["listen"].empty() || args["listen"].isArray()) {
        throw cocaine::error_t("no endpoints has been specified");
    }

    for(Json::Value::const_iterator it = args["listen"].begin();
        it != args["listen"].end();
        ++it)
    {
        std::string endpoint = (*it).asString();

        COCAINE_LOG_INFO(m_log, "listening on '%s'", endpoint);

        try {
            m_channel.bind(endpoint);
        } catch(const zmq::error_t& e) {
            throw configuration_error_t(
                "unable to bind at '%s' - %s",
                endpoint,
                e.what()
            );
        }
    }

    m_watcher.set<reactor_t, &reactor_t::on_event>(this);
    m_watcher.start(m_channel.fd(), ev::READ);
    m_checker.set<reactor_t, &reactor_t::on_check>(this);
    m_checker.start();

    m_terminate.set<reactor_t, &reactor_t::on_terminate>(this);
    m_terminate.start();
}

void
reactor_t::run() {
    BOOST_ASSERT(!m_thread);

    auto runnable = boost::bind(
        &ev::loop_ref::loop,
        boost::ref(m_loop),
        0
    );

    m_thread.reset(new boost::thread(runnable));
}

void
reactor_t::terminate() {
    BOOST_ASSERT(m_thread);

    m_terminate.send();

    m_thread->join();
    m_thread.reset();
}

void
reactor_t::on_event(ev::io&, int) {
    bool pending = false;

    m_checker.stop();

    {
        boost::unique_lock<io::shared_channel_t> lock(m_channel);
        pending = m_channel.pending();
    }

    if(pending) {
        m_checker.start();
        process();    
    }
}

void
reactor_t::on_check(ev::prepare&, int) {
    m_loop.feed_fd_event(m_channel.fd(), ev::READ);
}

void
reactor_t::on_terminate(ev::async&, int) {
    m_loop.unloop(ev::ALL);
}

void
reactor_t::process() {
    int counter = defaults::io_bulk_size;
    
    std::string source;
    int message_id;
    zmq::message_t message;
   
    bool rv;

    do {
        boost::unique_lock<io::shared_channel_t> lock(m_channel);

        {
            io::scoped_option<
                io::options::receive_timeout
            > option(m_channel, 0);
                
            try {
                rv = m_channel.recv_multipart(
                    io::protect(source),
                    message_id,
                    message
                );
            } catch(const cocaine::error_t& e) {
                m_channel.drop();
                continue;
            }
        }

        if(!rv) {
            // NOTE: Means the non-blocking read got nothing.
            return;
        }
        
        slot_map_t::const_iterator slot = m_slots.find(message_id);

        if(slot == m_slots.end()) {
            COCAINE_LOG_WARNING(m_log, "dropping an unknown message type %d", message_id);
            continue;
        }

        msgpack::unpacked unpacked;
        
        try {
            msgpack::unpack(
                &unpacked,
                static_cast<const char*>(message.data()),
                message.size()
            );
        } catch(const msgpack::unpack_error& e) {
            return;
        }

        const msgpack::object& request = unpacked.get();
        std::string response;

        try {
            response = (*slot->second)(request);
        } catch(const std::exception& e) {
            COCAINE_LOG_ERROR(
                m_log,
                "unable to process message type %d - %s",
                message_id,
                e.what()
            );

            return;
        }

        if(!response.empty()) {
            m_channel.send_multipart(
                io::protect(source),
                io::protect(response)
            );
        }
    } while(--counter);
}
