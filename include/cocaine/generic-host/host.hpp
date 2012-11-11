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

#ifndef COCAINE_GENERIC_HOST_HPP
#define COCAINE_GENERIC_HOST_HPP

#include <deque>

#include "cocaine/common.hpp"
#include "cocaine/asio.hpp"
#include "cocaine/rpc.hpp"
#include "cocaine/unique_id.hpp"

#include "cocaine/api/sandbox.hpp"

namespace cocaine {

class host_t;

struct request_t:
    public api::request_t
{
    request_t(const unique_id_t& id):
        api::request_t(id)
    { }

    void
    emit_chunk(const void * chunk,
               size_t size)
    {
        std::string blob(
            static_cast<const char*>(chunk),
            size
        );

        for(const auto& callback: m_chunk_chain) {
            callback(blob);
        }
    }

    void
    emit_close() {
        for(const auto& callback: m_close_chain) {
            callback();
        }
    }

    virtual
    void
    on_chunk(api::chunk_fn_t callback) {
        m_chunk_chain.emplace_back(callback);
    }

    virtual
    void
    on_close(api::close_fn_t callback) {
        m_close_chain.emplace_back(callback);
    }

private:
    typedef std::vector<
        api::chunk_fn_t
    > chunk_chain_t;

    chunk_chain_t m_chunk_chain;

    typedef std::vector<
        api::close_fn_t
    > close_chain_t;

    close_chain_t m_close_chain;
};

struct response_t:
    public api::response_t
{
    response_t(const unique_id_t& id,
               host_t * const host):
        api::response_t(id),
        m_host(host)
    { }

    virtual
    void
    write(const void * chunk,
          size_t size);

    virtual
    void
    close();

private:
    host_t * const m_host;
};

struct emitter_t:
    public api::emitter_t
{
    void
    emit(const std::string& event,
         const boost::shared_ptr<api::request_t>& request,
         const boost::shared_ptr<api::response_t>& response)
    {
        chain_t& chain = m_chains[event];

        for(const auto& callback: chain) {
            callback(request, response);
        }
    }

    virtual
    void
    on_event(const std::string& event,
             api::event_fn_t callback)
    {
        chain_t& chain = m_chains[event];

        // if(std::find_if(chain.begin(), chain.end(), equal()) != chain.end()) {
        //    throw cocaine::error_t("duplicate callback");
        // }

        chain.emplace_back(callback);
    }

private:
    typedef std::vector<
        api::event_fn_t
    > chain_t;

#if BOOST_VERSION >= 103600
    typedef boost::unordered_map<
#else
    typedef std::map<
#endif
        std::string,
        chain_t
    > chain_map_t;

    chain_map_t m_chains;
};

struct host_config_t {
    std::string name;
    std::string profile;
    std::string uuid;
};

class host_t:
    public boost::noncopyable
{
    public:
        host_t(context_t& context,
               host_config_t config);

        ~host_t();

        void
        run();

        // Response I/O

        template<class Event>
        void
        send(const io::message<Event>& message);

    private:
        void
        on_bus_event(ev::io&, int);
        
        void
        on_bus_check(ev::prepare&, int);
        
        void
        on_heartbeat(ev::timer&, int);

        void
        on_disown(ev::timer&, int);

        void
        on_idle(ev::timer&, int);
        
    private:
        void
        process_bus_events();
        
        void
        invoke(const unique_id_t& session_id,
               const std::string& event);
        
        void
        terminate(rpc::suicide::reasons reason,
                  const std::string& message);

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        const unique_id_t m_id;
        const std::string m_name;

        // Engine I/O

        typedef io::channel<
            rpc::rpc_plane_tag,
            io::policies::unique
        > rpc_channel_t;

        rpc_channel_t m_bus;
        
        // Event loop

        ev::default_loop m_loop;
        
        ev::io m_watcher;
        ev::prepare m_checker;
        
        ev::timer m_heartbeat_timer,
                  m_disown_timer,
                  m_idle_timer;
        
        // The app

        std::unique_ptr<const manifest_t> m_manifest;
        std::unique_ptr<const profile_t> m_profile;
        std::unique_ptr<api::sandbox_t> m_sandbox;

        // The event emitter
        emitter_t m_emitter;

        // Session map
#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            unique_id_t,
            boost::shared_ptr<request_t>
        > request_map_t;

        request_map_t m_requests;
};

template<class Event>
void
host_t::send(const io::message<Event>& message) {
    m_bus.send_message(message);
}

} // namespace cocaine::engine

#endif
