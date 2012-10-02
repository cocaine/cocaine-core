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

#include "cocaine/slave/slave.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/manifest.hpp"
#include "cocaine/profile.hpp"
#include "cocaine/rpc.hpp"

using namespace cocaine;
using namespace cocaine::engine;

namespace fs = boost::filesystem;

slave_t::slave_t(context_t& context, slave_config_t config):
    unique_id_t(config.uuid),
    m_context(context),
    m_log(context.log(
        (boost::format("app/%1%")
            % config.name
        ).str()
    )),
    m_name(config.name),
    m_bus(context, config.uuid),
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
            % m_name
        ).str()
    );
    
    m_watcher.set<slave_t, &slave_t::message>(this);
    m_watcher.start(m_bus.fd(), ev::READ);
    m_checker.set<slave_t, &slave_t::check>(this);
    m_checker.start();

    m_heartbeat_timer.set<slave_t, &slave_t::heartbeat>(this);
    m_heartbeat_timer.start(0.0f, 5.0f);

    // Launching the app
    // -----------------

    try {
        m_manifest.reset(new manifest_t(m_context, m_name));
        m_profile.reset(new profile_t(m_context, config.profile));
        
        m_idle_timer.set<slave_t, &slave_t::timeout>(this);
        m_idle_timer.start(m_profile->idle_timeout);
        
        fs::path path(
            fs::path(m_context.config.spool_path) / m_name
        );
         
        m_sandbox = m_context.get<api::sandbox_t>(
            m_manifest->type,
            api::category_traits<api::sandbox_t>::args_type(
                *m_manifest,
                path.string()
            )
        );
    } catch(const std::exception& e) {
        io::message<rpc::error> message(server_error, e.what());
        m_bus.send(m_name, message);
        terminate();
    } catch(...) {
        io::message<rpc::error> message(
            server_error,
            "unexpected exception while configuring the slave"
        );
        
        m_bus.send(m_name, message);
        terminate();
    }
}

slave_t::~slave_t() { }

void slave_t::run() {
    m_loop.loop();
}

std::string slave_t::read(int timeout) {
    zmq::message_t message;

    io::scoped_option<
        io::options::receive_timeout
    > option(m_bus, timeout);

    m_bus.recv(&message);
    
    return std::string(
        static_cast<const char*>(message.data()),
        message.size()
    );
}

void slave_t::write(const void * data, size_t size) {
    zmq::message_t body(size);

    memcpy(body.data(), data, size);
    io::message<rpc::chunk> message(body);

    m_bus.send(m_name, message);
}

void slave_t::message(ev::io&, int) {
    m_checker.stop();

    if(m_bus.pending()) {
        m_checker.start();
        process();
    }
}

void slave_t::check(ev::prepare&, int) {
    m_loop.feed_fd_event(m_bus.fd(), ev::READ);
}

void slave_t::process() {
    // TEST: Ensure that we haven't missed something in a previous iteration.
    BOOST_ASSERT(!m_bus.more());
    
    std::string source; 
    int command = 0;

    boost::tuple<
        io::raw<std::string>,
        int&
    > proxy(io::protect(source), command);
   
    if(!m_bus.recv_tuple(proxy, ZMQ_NOBLOCK)) {
        return;
    }

    m_log->debug(
        "slave %s got type %d command from engine %s",
        id().c_str(),
        command,
        source.c_str()
    );

    switch(command) {
        case io::get<rpc::invoke>::value: {
            // TEST: Ensure that we have the app first.
            BOOST_ASSERT(m_sandbox.get() != NULL);

            std::string event;

            m_bus.recv(event);
            invoke(event);
            
            break;
        }
        
        case io::get<rpc::terminate>::value:
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

void slave_t::timeout(ev::timer&, int) {
    terminate();
}

void slave_t::heartbeat(ev::timer&, int) {
    if(!m_bus.send(m_name, io::message<rpc::heartbeat>())) {
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
    } catch(const unrecoverable_error_t& e) {
        io::message<rpc::error> message(server_error, e.what());
        m_bus.send(m_name, message);
    } catch(const std::exception& e) {
        io::message<rpc::error> message(app_error, e.what());
        m_bus.send(m_name, message);
    } catch(...) {
        io::message<rpc::error> message(
            server_error,
            "unexpected exception while processing an event"
        );
        
        m_bus.send(m_name, message);
    }
    
    m_bus.send(m_name, io::message<rpc::choke>());
    
    // NOTE: Drop all the outstanding request chunks not pulled
    // in by the user code. Might have a warning here?
    m_bus.drop();

    m_idle_timer.stop();
    m_idle_timer.start(m_profile->idle_timeout);
}

void slave_t::terminate() {
    m_bus.send(m_name, io::message<rpc::terminate>());
    m_loop.unloop(ev::ALL);
}
