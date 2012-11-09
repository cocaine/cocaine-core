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
#include "cocaine/io.hpp"
#include "cocaine/unique_id.hpp"

#include "cocaine/api/sandbox.hpp"

namespace cocaine {

struct host_config_t {
    std::string name;
    std::string profile;
    std::string uuid;
};

class host_t:
    public boost::noncopyable,
    public api::io_t
{
    enum states: int {
        idle,
        active
    };

    public:
        host_t(context_t& context,
               host_config_t config);

        ~host_t();

        enum modes: int {
            normal   = 0,
            oneshot  = ev::ONESHOT,
            nonblock = ev::NONBLOCK
        };

        void
        run(modes mode = modes::normal);

        // I/O object implementation
        
        virtual
        std::string
        read(int timeout);

        virtual
        void
        write(const void * data,
              size_t size);

    private:
        void
        on_event(ev::io&, int);
        
        void
        on_check(ev::prepare&, int);
        
        void
        on_heartbeat(ev::timer&, int);

        void
        on_disown(ev::timer&, int);

        void
        on_idle(ev::timer&, int);
        
    private:
        void
        process_events();
        
        void
        invoke(const std::string& event);
        
        void
        terminate();

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        const unique_id_t m_id;
        const std::string m_name;

        states m_state;

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

        // Chunk cache

        typedef std::deque<
            std::string
        > chunk_queue_t;

        chunk_queue_t m_queue;
};

} // namespace cocaine::engine

#endif
