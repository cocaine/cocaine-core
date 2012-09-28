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

#ifndef COCAINE_SERVER_HPP
#define COCAINE_SERVER_HPP

#include "cocaine/common.hpp"

// Has to be included after common.h
#include <ev++.h>

#include "cocaine/auth.hpp"
#include "cocaine/io.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine {

struct server_config_t {
    // Runlist name for this server instance.
    std::string runlist;

    // Control and multicast endpoints.
    std::vector<std::string> listen_endpoints,
                             announce_endpoints;
    
    // Multicast announce interval.
    float announce_interval;
};

class server_t:
    public boost::noncopyable
{
    public:
        server_t(context_t& context,
                 server_config_t config);

        ~server_t();

        void run();

    private:        
        // Signal handling.
        void terminate(ev::sig&, int);
        void reload(ev::sig&, int);

        // I/O handling.
        void request(ev::io&, int);
        void process(ev::idle&, int);
        void check(ev::prepare&, int);

        // JSON-RPC command dispatching.
        std::string dispatch(const Json::Value& root);

        Json::Value create_app(const std::string& name, const std::string& profile);
        Json::Value delete_app(const std::string& name);
        Json::Value info() const;

        // Multicast-based node announces.
        void announce(ev::timer&, int);

        // App runlist revalidation.
        void recover();

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        // Apps.
        const std::string m_runlist;

#if BOOST_VERSION >= 104000
        typedef boost::ptr_unordered_map<
#else
        typedef boost::ptr_map<
#endif
            const std::string,
            app_t
        > app_map_t;

        app_map_t m_apps;

        // Event loop.
        ev::default_loop m_loop;

        // Event watchers.
        ev::sig m_sigint,
                m_sigterm,
                m_sigquit, 
                m_sighup;
                
        ev::io m_watcher;
        ev::idle m_processor;
        ev::prepare m_check;

        // System I/O.
        io::socket_t m_server;

        // Automatic discovery support.
        std::auto_ptr<ev::timer> m_announce_timer;
        std::auto_ptr<io::socket_t> m_announces;
        
        // Authorization subsystem.
        crypto::auth_t m_auth;
        
        // Server uptime.
        const ev::tstamp m_birthstamp;

        // Info cache.
        ev::tstamp m_infostamp;
        std::string m_infocache;
};

} // namespace cocaine

#endif
