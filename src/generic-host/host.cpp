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

#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>

#include "cocaine/generic-host/host.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/profile.hpp"
#include "cocaine/rpc.hpp"

#include "cocaine/traits/unique_id.hpp"

using namespace cocaine;
using namespace cocaine::engine;

namespace fs = boost::filesystem;

host_t::host_t(context_t& context,
               host_config_t config):
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % config.name
        ).str()
    )),
    m_id(config.uuid),
    m_name(config.name),
    m_bus(context, ZMQ_DEALER, m_id)
{
    std::string endpoint(
        (boost::format("ipc://%1%/%2%")
            % m_context.config.ipc_path
            % m_name
        ).str()
    );
    
    m_bus.connect(endpoint);
    
    m_watcher.set<host_t, &host_t::on_event>(this);
    m_watcher.start(m_bus.fd(), ev::READ);
    m_checker.set<host_t, &host_t::on_check>(this);
    m_checker.start();

    m_heartbeat_timer.set<host_t, &host_t::on_heartbeat>(this);
    m_heartbeat_timer.start(0.0f, 5.0f);

    // NOTE: It will be restarted after the each heartbeat.
    m_disown_timer.set<host_t, &host_t::on_disown>(this);

    // Launching the app

    try {
        m_manifest.reset(new manifest_t(m_context, m_name));
        m_profile.reset(new profile_t(m_context, config.profile));
        
        fs::path path(fs::path(m_context.config.spool_path) / m_name);
         
        m_sandbox = m_context.get<api::sandbox_t>(
            m_manifest->sandbox.type,
            api::category_traits<api::sandbox_t>::args_type(
                m_manifest->name,
                m_manifest->sandbox.args,
                path.string()
            )
        );
    } catch(const std::exception& e) {
        m_bus.send_message(
            io::message<rpc::error>(server_error, e.what())
        );
        
        terminate();

        // Rethrow so that the slave would terminate instead of looping once.
        throw;
    } catch(...) {
        m_bus.send_message(
            io::message<rpc::error>(
                server_error,
                "unexpected exception while configuring the generic host"
            )
        );

        terminate();
        
        // Rethrow so that the slave would terminate instead of looping once.
        throw;
    }

    m_idle_timer.set<host_t, &host_t::on_idle>(this);
    m_idle_timer.start(m_profile->idle_timeout);        
}

host_t::~host_t() {
    // Empty.
}

void
host_t::run(int timeout) {
    io::scoped_option<
        io::options::receive_timeout,
        io::policies::unique
    > option(m_bus, timeout);

    m_loop.loop();
}

std::string
host_t::read(int timeout) {
    // Consume all the queued events and block for 'timeout' if no chunks
    // arrived yet.
    run(timeout);

    if(!m_queue.empty()) {
        std::string chunk(m_queue.front());
        m_queue.pop_front();

        return chunk;
    }

    throw cocaine::error_t("timed out");
}

void
host_t::write(const void * data,
              size_t size)
{
    zmq::message_t message(size);

    memcpy(
        message.data(),
        data,
        size
    );
    
    m_bus.send_message(io::message<rpc::chunk>(message));

    // Consume all the queued events, don't block.
    run();
}

void
host_t::on_event(ev::io&, int) {
    m_checker.stop();

    if(m_bus.pending()) {
        m_checker.start();
        process_events();
    }
}

void
host_t::on_check(ev::prepare&, int) {
    m_loop.feed_fd_event(m_bus.fd(), ev::READ);
}

void
host_t::process_events() {
    // TEST: Ensure that we haven't missed something in a previous iteration.
    BOOST_ASSERT(!m_bus.more());
   
    int command = -1;

    if(!m_bus.recv(command)) {
        // If this is not the outermost event loop and nothing left in the queue,
        // break it and return control to the user code.
        if(m_loop.depth() > 1) {
            m_loop.unloop(ev::ONE);
        }

        return;
    }

    COCAINE_LOG_DEBUG(
        m_log,
        "generic host %s received type %d message",
        m_id,
        command
    );

    switch(command) {
        case io::message<rpc::pong>::value:
            m_disown_timer.stop();
            break;

        case io::message<rpc::invoke>::value: {
            unique_id_t job_id;
            std::string event;

            m_bus.recv_tuple(boost::tie(job_id, event));
            
            invoke(event);
            
            break;
        }
        
        case io::message<rpc::chunk>::value: {
            zmq::message_t body;
    
            BOOST_ASSERT(m_bus.more());

            m_bus.recv(&body);

            m_queue.emplace_back(
                static_cast<const char*>(body.data()),
                body.size()
            );
        }

        case io::message<rpc::choke>::value:
            throw cocaine::error_t("the session has been closed");
        
        case io::message<rpc::terminate>::value:
            terminate();
            break;

        default:
            COCAINE_LOG_WARNING(
                m_log,
                "generic host %s dropping unknown type %d message", 
                m_id,
                command
            );
            
            m_bus.drop();
    }
}

void
host_t::on_heartbeat(ev::timer&, int) {
    m_bus.send_message(io::message<rpc::ping>());
    m_disown_timer.start(5.0f);
}

void
host_t::on_disown(ev::timer&, int) {
    COCAINE_LOG_ERROR(
        m_log,
        "generic host %s has lost the controlling engine",
        m_id
    );

    m_loop.unloop();    
}

void
host_t::on_idle(ev::timer&, int) {
    terminate();
}

void
host_t::invoke(const std::string& event) {
    // TEST: Ensure that we have the app first.
    BOOST_ASSERT(m_sandbox.get() != NULL);

    // Pause the idle timer. 
    m_idle_timer.stop();

    try {
        m_sandbox->invoke(event, *this);
    } catch(const unrecoverable_error_t& e) {
        io::message<rpc::error> message(server_error, e.what());
        m_bus.send_message(message);
    } catch(const std::exception& e) {
        io::message<rpc::error> message(app_error, e.what());
        m_bus.send_message(message);
    } catch(...) {
        io::message<rpc::error> message(
            server_error,
            "unexpected exception while processing an event"
        );
        
        m_bus.send_message(message);
    }
    
    m_bus.send_message(io::message<rpc::choke>());
   
    // Rearm the idle timer.
    m_idle_timer.start(m_profile->idle_timeout);

    // Feed the event loop.
    m_loop.feed_fd_event(m_bus.fd(), ev::READ);
}

void
host_t::terminate() {
    m_bus.send_message(io::message<rpc::terminate>());
    m_loop.unloop(ev::ALL);
}
