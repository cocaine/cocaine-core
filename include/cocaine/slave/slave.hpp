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

// Has to be included after common.h
#include <ev++.h>

#include "cocaine/io.hpp"

#include "cocaine/api/sandbox.hpp"

#include "cocaine/helpers/unique_id.hpp"

namespace cocaine { namespace engine {

struct slave_config_t {
    std::string name;
    std::string profile;
    std::string uuid;
};

class slave_t:
    public boost::noncopyable,
    public unique_id_t,
    public api::io_t
{
    public:
        slave_t(context_t& context,
                slave_config_t config);

        ~slave_t();

        void run();

        // I/O object implementation
        // -------------------------
        
        virtual std::string read(int timeout);

        virtual void write(const void * data,
                           size_t size);
        
    private:
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void check(ev::prepare&, int);
        void timeout(ev::timer&, int);
        void heartbeat(ev::timer&, int);

        // Dispatching.
        void invoke(const std::string& event);
        void terminate();

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        // The app.
        const std::string m_name;

        std::auto_ptr<const manifest_t> m_manifest;
        std::auto_ptr<const profile_t> m_profile;

        std::auto_ptr<api::sandbox_t> m_sandbox;

        // Event loop.
        ev::default_loop m_loop;
        
        ev::io m_watcher;
        ev::idle m_processor;
        ev::prepare m_check;
        
        ev::timer m_heartbeat_timer,
                  m_idle_timer;
        
        // Engine RPC.
        io::channel_t m_bus;
        io::scoped_option<io::options::send_timeout> m_bus_timeout;
};

}} // namespace cocaine::engine

#endif
