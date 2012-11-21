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

#include "cocaine/common.hpp"
#include "cocaine/asio.hpp"
#include "cocaine/rpc.hpp"
#include "cocaine/unique_id.hpp"

#include "cocaine/api/stream.hpp"

namespace cocaine {

struct host_config_t {
    std::string name;
    std::string profile;
    std::string uuid;
};

class host_t:
    public boost::noncopyable
{
    struct io_pair_t {
        boost::shared_ptr<api::stream_t> upstream;
        boost::shared_ptr<api::stream_t> downstream;
    };

#if BOOST_VERSION >= 103600
    typedef boost::unordered_map<
#else
    typedef std::map<
#endif
        unique_id_t,
        io_pair_t
    > stream_map_t;

    public:
        host_t(context_t& context,
               host_config_t config);

        ~host_t();

        void
        run();

        template<class Event, typename... Args>
        void
        send(Args&&... args);

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
        process_bus_events();
        
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
            tags::rpc_tag,
            io::policies::unique
        > rpc_channel_t;

        rpc_channel_t m_bus;
        
        // Event loop

        ev::default_loop m_loop;
        
        ev::io m_watcher;
        ev::prepare m_checker;
        
        ev::timer m_heartbeat_timer,
                  m_disown_timer;
        
        // The app

        std::unique_ptr<const manifest_t> m_manifest;
        std::unique_ptr<const profile_t> m_profile;
        std::unique_ptr<api::sandbox_t> m_sandbox;

        // Session streams

        stream_map_t m_streams;
};

template<class Event, typename... Args>
void
host_t::send(Args&&... args) {
    m_bus.send_messagex<Event>(std::forward<Args>(args)...);
}

} // namespace cocaine::engine

#endif
