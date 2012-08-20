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

#include <boost/format.hpp>

#include "cocaine/slave/slave.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/rpc.hpp"

using namespace cocaine;
using namespace cocaine::engine;

slave_t::slave_t(context_t& context, slave_config_t config):
    unique_id_t(config.uuid),
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % config.app
        ).str()
    )),
    m_app(config.app),
    m_bus(m_context.io(), config.uuid),
    m_bus_timeout(m_bus, defaults::bus_timeout)
{
    int linger = 0;

    m_bus.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

#ifdef ZMQ_ROUTER_BEHAVIOR
    int mode = 1;
    
    m_bus.setsockopt(ZMQ_ROUTER_BEHAVIOR, &mode, sizeof(mode));
#endif

    m_bus.connect(
        (boost::format("ipc://%1%/%2%")
            % m_context.config.ipc_path
            % config.app
        ).str()
    );
    
    m_watcher.set<slave_t, &slave_t::message>(this);
    m_watcher.start(m_bus.fd(), ev::READ);
    m_processor.set<slave_t, &slave_t::process>(this);
    m_check.set<slave_t, &slave_t::check>(this);
    m_check.start();

    m_heartbeat_timer.set<slave_t, &slave_t::heartbeat>(this);
    m_heartbeat_timer.start(0.0f, 5.0f);

    configure();
}

slave_t::~slave_t() { }

void slave_t::configure() {
    try {
        m_manifest.reset(new manifest_t(m_context, m_app));
        
        m_idle_timer.set<slave_t, &slave_t::timeout>(this);
        m_idle_timer.start(m_manifest->policy.idle_timeout);
        
        m_sandbox = m_context.get<api::sandbox_t>(
            m_manifest->type,
            api::category_traits<api::sandbox_t>::args_type(*m_manifest)
        );
    } catch(const configuration_error_t& e) {
        io::command<rpc::error> command(server_error, e.what());
        m_bus.send(m_app, command);
        terminate();
    } catch(const repository_error_t& e) {
        io::command<rpc::error> command(server_error, e.what());
        m_bus.send(m_app, command);
        terminate();
    } catch(const unrecoverable_error_t& e) {
        io::command<rpc::error> command(server_error, e.what());
        m_bus.send(m_app, command);
        terminate();
    } catch(const std::exception& e) {
        io::command<rpc::error> command(server_error, e.what());
        m_bus.send(m_app, command);
        terminate();
    } catch(...) {
        io::command<rpc::error> command(
            server_error,
            "unexpected exception while configuring the slave"
        );
        
        m_bus.send(m_app, command);
        terminate();
    }
}

void slave_t::run() {
    m_loop.loop();
}

blob_t slave_t::read(int timeout) {
    zmq::message_t message;

    io::scoped_option<
        io::options::receive_timeout
    > option(m_bus, timeout);

    m_bus.recv(&message);
    
    return blob_t(message.data(), message.size());
}

void slave_t::write(const void * data, size_t size) {
    io::command<rpc::chunk> command(data, size);
    m_bus.send(m_app, command);
}

void slave_t::message(ev::io&, int) {
    if(m_bus.pending() && !m_processor.is_active()) {
        m_processor.start();
    }
}

void slave_t::process(ev::idle&, int) {
    // TEST: Ensure that we haven't missed something in a previous iteration.
    BOOST_ASSERT(!m_bus.more());
    
    std::string source; 
    int command = 0;

    boost::tuple<
        io::raw<std::string>,
        int&
    > proxy(io::protect(source), command);
   
    {
        io::scoped_option<io::options::receive_timeout> option(m_bus, 0);

        if(!m_bus.recv_multi(proxy)) {
            m_processor.stop();
            return;
        }
    }

    m_log->debug(
        "got type %d command from engine %s",
        command,
        source.c_str()
    );

    switch(command) {
        case io::get<rpc::invoke>::id::value: {
            // TEST: Ensure that we have the app first.
            BOOST_ASSERT(m_sandbox.get() != NULL);

            std::string event;

            m_bus.recv(event);
            invoke(event);
            
            break;
        }
        
        case io::get<rpc::terminate>::id::value:
            terminate();
            break;

        default:
            m_log->warning(
                "slave %s dropping unknown type %d command from engine %s", 
                id().c_str(),
                command,
                source.c_str()
            );
            
            m_bus.drop();
    }
}

void slave_t::check(ev::prepare&, int) {
    message(m_watcher, ev::READ);
}

void slave_t::timeout(ev::timer&, int) {
    terminate();
}

void slave_t::heartbeat(ev::timer&, int) {
    if(!m_bus.send(m_app, io::command<rpc::heartbeat>())) {
        m_log->error(
            "slave %s has lost the controlling engine",
            id().c_str()
        );

        terminate();
    }
}

void slave_t::invoke(const std::string& event) {
    try {
        m_sandbox->invoke(event, *this);
    } catch(const recoverable_error_t& e) {
        io::command<rpc::error> command(app_error, e.what());
        m_bus.send(m_app, command);
    } catch(const unrecoverable_error_t& e) {
        io::command<rpc::error> command(server_error, e.what());
        m_bus.send(m_app, command);
    } catch(const std::exception& e) {
        io::command<rpc::error> command(app_error, e.what());
        m_bus.send(m_app, command);
    } catch(...) {
        io::command<rpc::error> command(
            server_error,
            "unexpected exception while processing an event"
        );
        
        m_bus.send(m_app, command);
    }
    
    m_bus.send(m_app, io::command<rpc::choke>());
    
    // NOTE: Drop all the outstanding request chunks not pulled
    // in by the user code. Might have a warning here?
    m_bus.drop();

    m_idle_timer.stop();
    m_idle_timer.start(m_manifest->policy.idle_timeout);
}

void slave_t::terminate() {
    m_bus.send(m_app, io::command<rpc::terminate>());
    m_loop.unloop(ev::ALL);
}
