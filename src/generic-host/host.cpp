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

#include "cocaine/api/sandbox.hpp"

#include "cocaine/traits/unique_id.hpp"

using namespace cocaine;
using namespace cocaine::engine;

namespace fs = boost::filesystem;

struct upstream_t:
    public api::stream_t
{
    upstream_t(const unique_id_t& id,
               host_t * const host):
        m_id(id),
        m_host(host),
        m_state(attached)
    { }

    virtual
    void
    push(const void * chunk,
         size_t size)
    {
        if(m_state == closed) {
            throw cocaine::error_t("the stream has been closed");
        }

        zmq::message_t message(size);

        memcpy(
            message.data(),
            chunk,
            size
        );

        m_host->send(io::message<rpc::chunk>(m_id, message));        
    }

    virtual
    void
    error(error_code code,
          const std::string& message)
    {
        if(m_state == closed) {
            throw cocaine::error_t("the stream has been closed");
        }

        m_host->send(io::message<rpc::error>(m_id, code, message));

        close();        
    }

    virtual
    void
    close() {
        if(m_state == closed) {
            throw cocaine::error_t("the stream has been closed");
        }

        m_host->send(io::message<rpc::choke>(m_id));

        m_state = states::closed;
    }

private:
    const unique_id_t m_id;
    host_t * const m_host;

    enum states: int {
        attached,
        closed
    };

    states m_state;
};

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
            path.string()
        );
    } catch(const std::exception& e) {
        terminate(rpc::suicide::abnormal, e.what());
        throw;
    } catch(...) {
        terminate(rpc::suicide::abnormal, "unexpected exception");
        throw;
    }
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
    m_disown_timer.start(m_profile->heartbeat_timeout);
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

                stream_map_t::iterator it(m_streams.find(session_id));

                // NOTE: This may be a chunk for a failed invocation, in which case there
                // will be no active stream, so drop the message.
                if(it != m_streams.end()) {
                    try {
                        it->second.downstream->push(message.data(), message.size());
                    } catch(const std::exception& e) {
                        it->second.upstream->error(invocation_error, e.what());
                        m_streams.erase(it);
                    } catch(...) {
                        it->second.upstream->error(invocation_error, "unexpected exception");
                        m_streams.erase(it);
                    }
                }

                break;
            }

            case io::message<rpc::choke>::value: {
                unique_id_t session_id(uninitialized);

                m_bus.recv(session_id);

                stream_map_t::iterator it = m_streams.find(session_id);

                // NOTE: This may be a choke for a failed invocation, in which case there
                // will be no active stream, so drop the message.
                if(it != m_streams.end()) {
                    try {
                        it->second.downstream->close();
                    } catch(const std::exception& e) {
                        it->second.upstream->error(invocation_error, e.what());
                    } catch(...) {
                        it->second.upstream->error(invocation_error, "unexpected exception");
                    }
                    
                    m_streams.erase(it);
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
    boost::shared_ptr<api::stream_t> upstream(
        boost::make_shared<upstream_t>(session_id, this)
    );

    try {
        io_pair_t io = {
            upstream,
            m_sandbox->invoke(event, upstream)
        };

        m_streams.emplace(session_id, io);
    } catch(const std::exception& e) {
        upstream->error(invocation_error, e.what());
    } catch(...) {
        upstream->error(invocation_error, "unexpected exception");
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
