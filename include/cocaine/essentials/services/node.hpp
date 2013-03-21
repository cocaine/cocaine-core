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

#ifndef COCAINE_NODE_SERVICE_HPP
#define COCAINE_NODE_SERVICE_HPP

#include "cocaine/common.hpp"
#include "cocaine/actor.hpp"
#include "cocaine/json.hpp"

#include <zmq.hpp>

namespace cocaine {

namespace io {
    namespace tags {
        struct node_tag;
    }

    namespace node {
        struct start_app {
            typedef tags::node_tag tag;

            typedef boost::mpl::list<
                /* runlist */ std::map<std::string, std::string>
            > tuple_type;
        };

        struct pause_app {
            typedef tags::node_tag tag;

            typedef boost::mpl::list<
                /* applist */ std::vector<std::string>
            > tuple_type;
        };

        struct info {
            typedef tags::node_tag tag;
        };
    }

    template<>
    struct protocol<tags::node_tag> {
        typedef mpl::list<
            node::start_app,
            node::pause_app,
            node::info
        > type;
    };
} // namespace io

namespace service {

class node_t:
    public reactor_t
{
    public:
        node_t(context_t& context,
               const std::string& name,
               const Json::Value& args);

        virtual
        ~node_t();

    private:
        void
        on_announce(ev::timer&, int);

        Json::Value
        on_start_app(const std::map<std::string, std::string>& runlist);

        Json::Value
        on_pause_app(const std::vector<std::string>& applist);

        Json::Value
        on_info() const;

    private:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;

        // Node announce channel.
        // DEPRECATED: Drop when Dealer switches to ASIO.
        zmq::context_t m_zmq_context;
        zmq::socket_t m_announces;

        // Node announce timer.
        ev::timer m_announce_timer;

#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            std::string,
            std::shared_ptr<app_t>
        > app_map_t;

        // Apps.
        app_map_t m_apps;

        // Uptime.
        const ev::tstamp m_birthstamp;
};

} // namespace service

} // namespace cocaine

#endif
