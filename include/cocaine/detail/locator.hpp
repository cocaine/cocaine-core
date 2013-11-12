/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2013 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#include <mutex>

namespace ev {
    struct io;
    struct timer;
}

namespace cocaine {

class locator_t:
    public implementation<io::locator_tag>
{
    public:
        locator_t(context_t& context, io::reactor_t& reactor);

        virtual
       ~locator_t();

    private:
        void
        connect();

        typedef io::event_traits<io::locator::resolve>::result_type resolve_result_type;
        typedef io::event_traits<io::locator::synchronize>::result_type synchronize_result_type;
        typedef io::event_traits<io::locator::refresh>::result_type refresh_result_type;

        auto
        resolve(const std::string& name) const -> resolve_result_type;

        auto
        refresh(const std::string& name) -> refresh_result_type;

        // Cluster I/O

        void
        on_announce_event(ev::io&, int);

        void
        on_announce_timer(ev::timer&, int);

        typedef std::tuple<std::string, std::string, uint16_t> key_type;

        void
        on_message(const key_type& key, const io::message_t& message);

        void
        on_failure(const key_type& key, const std::error_code& ec);

        void
        on_timeout(const key_type& key);

        void
        on_lifetap(const key_type& key);

    private:
        context_t& m_context;

        const std::unique_ptr<logging::log_t> m_log;

        // For cluster interconnections.
        io::reactor_t& m_reactor;

        // Announce receiver.
        std::unique_ptr<io::socket<io::udp>> m_sink;
        std::unique_ptr<ev::io> m_sink_watcher;

        struct remote_t {
            std::shared_ptr<io::channel<io::socket<io::tcp>>> channel;

            // Remote node timers.
            std::shared_ptr<io::timeout_t> lifetap;
            std::shared_ptr<io::timeout_t> timeout;
        };

        // These are remote channels indexed by endpoint and uuid. The uuid is required to easily
        // disambiguate between different runtime instances on the same host.
        std::map<key_type, remote_t> m_remotes;

        // Remote gateway.
        std::unique_ptr<api::gateway_t> m_gateway;

        // Announce emitter.
        std::unique_ptr<io::socket<io::udp>> m_announce;
        std::unique_ptr<ev::timer> m_announce_timer;

        class router_t;

        // Used to resolve service names against service groups based on weights and other metrics.
        std::unique_ptr<router_t> m_router;
};

} // namespace cocaine

#endif
