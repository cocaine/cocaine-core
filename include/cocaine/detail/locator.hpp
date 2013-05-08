/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_SERVICE_LOCATOR_HPP
#define COCAINE_SERVICE_LOCATOR_HPP

#include "cocaine/common.hpp"
#include "cocaine/dispatch.hpp"
#include "cocaine/messages.hpp"

namespace ev {
    struct io;
    struct timer;
}

namespace cocaine {

class actor_t;

typedef io::event_traits<io::locator::resolve>::result_type resolve_result_type;
typedef io::event_traits<io::locator::dump>::result_type dump_result_type;

class locator_t:
    public dispatch_t
{
    public:
        locator_t(context_t& context, io::reactor_t& reactor);

        virtual
       ~locator_t();

        void
        attach(const std::string& name, std::unique_ptr<actor_t>&& service);

    private:
        resolve_result_type
        resolve(const std::string& name) const;

        dump_result_type
        dump() const;

        // Cluster discovery

        void
        on_announce_event(ev::io&, int);

        void
        on_announce_timer(ev::timer&, int);

        void
        on_response(const std::string& hostname, const io::message_t& message);

        void
        on_shutdown(const std::string& hostname, const std::error_code& ec);

    private:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;

        // For future cluster locator interconnections.
        io::reactor_t& m_reactor;

        typedef std::vector<
            std::pair<std::string, std::unique_ptr<actor_t>>
        > service_list_t;

        // NOTE: These are the instances of all the configured services, stored
        // as a vector of pairs to preserve the initialization order.
        service_list_t m_services;

        std::unique_ptr<io::socket<io::udp>> m_announce;
        std::unique_ptr<ev::timer> m_announce_timer;
        std::unique_ptr<ev::io> m_announce_watcher;

        typedef std::map<
            std::string,
            std::shared_ptr<io::channel<io::socket<io::tcp>>>
        > remote_map_t;

        remote_map_t m_remotes;
};

} // namespace cocaine

#endif
