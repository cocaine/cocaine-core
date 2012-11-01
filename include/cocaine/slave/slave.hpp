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

#ifndef COCAINE_SLAVE_HPP
#define COCAINE_SLAVE_HPP

#include "cocaine/common.hpp"
#include "cocaine/asio.hpp"
#include "cocaine/io.hpp"
#include "cocaine/unique_id.hpp"

#include "cocaine/api/sandbox.hpp"

namespace cocaine { namespace engine {

struct slave_config_t {
    std::string name;
    std::string profile;
    std::string uuid;
};

class slave_t:
    public boost::noncopyable,
    public api::io_t
{
    public:
        slave_t(context_t& context,
                slave_config_t config);

        ~slave_t();

        void
        run();

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

        // Engine I/O

        io::channel<io::policies::unique> m_bus;
        
        io::scoped_option<
            io::options::send_timeout,
            io::policies::unique
        > m_bus_timeout;
        
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
};

}} // namespace cocaine::engine

#endif
