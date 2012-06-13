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

#include "cocaine/interfaces/sandbox.hpp"

#include "cocaine/helpers/blob.hpp"
#include "cocaine/helpers/unique_id.hpp"

namespace cocaine { namespace engine {

struct slave_config_t {
    std::string app;
    std::string uuid;
};

class slave_t:
    public boost::noncopyable,
    public unique_id_t,
    public io_t
{
    public:
        slave_t(context_t& context,
                   slave_config_t config);

        ~slave_t();

        void configure(const std::string& app);
        void run();

        template<class Packed>
        void send(Packed& packed) {
            m_bus.send_multi(packed);
        }

        // I/O object implementation.
        virtual blob_t read(int timeout);
        virtual void write(const void * data, size_t size);

    private:
        void message(ev::io&, int);
        void process(ev::idle&, int);
        void check(ev::prepare&, int);
        // void pump(ev::timer&, int);
        void timeout(ev::timer&, int);
        void heartbeat(ev::timer&, int);

        // Dispatching.
        void invoke(const std::string& event);
        void terminate();

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        // The app.
        std::auto_ptr<const manifest_t> m_manifest;
        std::auto_ptr<sandbox_t> m_sandbox;

        // Event loop.
        ev::default_loop m_loop;
        
        ev::io m_watcher;
        ev::idle m_processor;
        ev::prepare m_check;
        
        // XXX: This is a temporary workaround for the edge cases when ZeroMQ for some 
        // reason doesn't trigger the socket's fd on message arrival (or I poll it in a wrong way).
        // ev::timer m_pumper;

        ev::timer m_heartbeat_timer,
                  m_suicide_timer;
        
        // Engine RPC.
        io::channel_t m_bus;
};

}}

#endif
