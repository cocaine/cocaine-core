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

#include "cocaine/traits/unique_id.hpp"

using namespace cocaine;
using namespace cocaine::engine;

namespace fs = boost::filesystem;

void
response_t::write(const void * chunk,
                  size_t size)
{
    zmq::message_t message(size);

    memcpy(
        message.data(),
        chunk,
        size
    );

    m_host->send(io::message<rpc::chunk>(id, message));
}

void
response_t::close() {
    m_host->send(io::message<rpc::choke>(id));
}

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
    
    m_watcher.set<host_t, &host_t::on_bus_event>(this);
    m_watcher.start(m_bus.fd(), ev::READ);
    m_checker.set<host_t, &host_t::on_bus_check>(this);
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
            m_manifest->name,
            m_manifest->sandbox.args,
            path.string(),
            m_emitter
        );
    } catch(const std::exception& e) {
        terminate(rpc::suicide::abnormal, e.what());
        throw;
    } catch(...) {
        terminate(rpc::suicide::abnormal, "unexpected exception while configuring the sandbox");
        throw;
    }

    m_idle_timer.set<host_t, &host_t::on_idle>(this);
    m_idle_timer.start(m_profile->idle_timeout);        
}

host_t::~host_t() {
    // Empty.
}

void
host_t::run() {
    m_loop.loop();
}

void
host_t::on_bus_event(ev::io&, int) {
    m_checker.stop();

    if(m_bus.pending()) {
        m_checker.start();
        process_bus_events();
    }
}

void
host_t::on_bus_check(ev::prepare&, int) {
    m_loop.feed_fd_event(m_bus.fd(), ev::READ);
}

void
host_t::on_heartbeat(ev::timer&, int) {
    m_bus.send_message(io::message<rpc::ping>());
    m_disown_timer.start(60.0f);
}

void
host_t::on_disown(ev::timer&, int) {
    COCAINE_LOG_ERROR(
        m_log,
        "generic host %s has lost the controlling engine",
        m_id
    );

    m_loop.unloop(ev::ALL);    
}

void
host_t::on_idle(ev::timer&, int) {
    terminate(rpc::suicide::normal, "idle");
}

void
host_t::process_bus_events() {
    int counter = defaults::io_bulk_size;

    do {
        // TEST: Ensure that we haven't missed something in a previous iteration.
        BOOST_ASSERT(!m_bus.more());
       
        int command = -1;

        {
            io::scoped_option<
                io::options::receive_timeout,
                io::policies::unique
            > option(m_bus, 0);

            if(!m_bus.recv(command)) {
                return;
            }
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
                unique_id_t session_id;
                std::string event;

                m_bus.recv_tuple(boost::tie(session_id, event));
                
                invoke(session_id, event);
                
                break;
            }

            case io::message<rpc::chunk>::value: {
                unique_id_t session_id(uninitialized);
                zmq::message_t message;

                m_bus.recv_tuple(boost::tie(session_id, message));

                request_map_t::iterator it(m_requests.find(session_id));

                if(it == m_requests.end()) {
                    COCAINE_LOG_ERROR(m_log, "chunk: nonexistent session %s", session_id);
                } else {
                    it->second->emit_chunk(message.data(), message.size());
                }

                break;
            }

            case io::message<rpc::choke>::value: {
                unique_id_t session_id(uninitialized);

                m_bus.recv(session_id);

                request_map_t::iterator it = m_requests.find(session_id);

                if(it == m_requests.end()) {
                    COCAINE_LOG_ERROR(m_log, "choke: nonexistent session %s", session_id);
                } else {
                    it->second->emit_close();
                }

                m_requests.erase(it);

                if(m_requests.empty()) {
                    m_idle_timer.start(m_profile->idle_timeout);
                }

                break;
            }
            
            case io::message<rpc::terminate>::value:
                terminate(rpc::suicide::normal, "per request");
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
    } while(--counter);
}

void
host_t::invoke(const unique_id_t& session_id,
               const std::string& event)
{
    m_idle_timer.stop();

    request_map_t::iterator it;

    boost::tie(it, boost::tuples::ignore) = m_requests.emplace(
        session_id,
        boost::make_shared<request_t>(session_id)
    );

    boost::shared_ptr<response_t> response(
        boost::make_shared<response_t>(session_id, this)
    );

    try {
        m_emitter.emit(event, it->second, response);
    } catch(const unrecoverable_error_t& e) {
        send(io::message<rpc::error>(session_id, invocation_error, e.what()));
    } catch(const std::exception& e) {
        send(io::message<rpc::error>(session_id, invocation_error, e.what()));
    } catch(...) {
        send(
            io::message<rpc::error>(
                session_id,
                invocation_error,
                "unexpected exception while processing an event"
            )
        );
    }
    
    // Feed the event loop.
    m_loop.feed_fd_event(m_bus.fd(), ev::READ);
}

void
host_t::terminate(rpc::suicide::reasons reason,
                  const std::string& message)
{
    send(io::message<rpc::suicide>(reason, message));
    m_loop.unloop(ev::ALL);
}
