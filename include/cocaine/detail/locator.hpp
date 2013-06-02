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

#include "cocaine/detail/unique_id.hpp"

#include "cocaine/dispatch.hpp"
#include "cocaine/messages.hpp"

#include <random>

namespace ev {
    struct io;
    struct timer;
}

namespace cocaine {

class actor_t;

typedef io::event_traits<io::locator::resolve>::result_type resolve_result_type;
typedef io::event_traits<io::locator::synchronize>::result_type synchronize_result_type;

class locator_t:
    public dispatch_t
{
    struct remote_t {
        typedef std::tuple<
            std::string,
            std::string,
            uint16_t
        > key_type;

        std::shared_ptr<io::channel<io::socket<io::tcp>>> channel;
        std::shared_ptr<io::timeout_t> timeout;
    };

    struct synchronize_t;

    public:
        locator_t(context_t& context, io::reactor_t& reactor);

        virtual
       ~locator_t();

        void
        attach(const std::string& name, std::unique_ptr<actor_t>&& service);

    private:
        resolve_result_type
        resolve(const std::string& name) const;

        synchronize_result_type
        dump() const;

        // Cluster discovery

        void
        on_announce_event(ev::io&, int);

        void
        on_announce_timer(ev::timer&, int);

        void
        on_response(const remote_t::key_type& key, const io::message_t& message);

        void
        on_shutdown(const remote_t::key_type& key, const std::error_code& ec);

        void
        on_timedout(const remote_t::key_type& key);

        // Housekeeping

        void
        purge(const remote_t::key_type& key);

    private:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;

        // For future cluster locator interconnections.
        io::reactor_t& m_reactor;

        typedef std::vector<
            std::pair<std::string, std::unique_ptr<actor_t>>
        > local_service_list_t;

        // These are the instances of all the configured services, stored as a vector of pairs to
        // preserve the initialization order.
        local_service_list_t m_services;

        // Node's UUID in the cluster.
        unique_id_t m_id;

        // Announce emitter.
        std::unique_ptr<io::socket<io::udp>> m_announce;
        std::unique_ptr<ev::timer> m_announce_timer;

        // Synchronizing slot.
        std::shared_ptr<synchronize_t> m_synchronizer;

        // Announce receiver.
        std::unique_ptr<io::socket<io::udp>> m_sink;
        std::unique_ptr<ev::io> m_sink_watcher;

        typedef std::map<
            remote_t::key_type,
            remote_t
        > remote_map_t;

        // These are remote channels indexed by (endpoint, id) tuples. The unique id is required to
        // distinguish between different runtime instances on the same host.
        remote_map_t m_remotes;

#if defined(__clang__) || defined(HAVE_GCC46)
        mutable std::default_random_engine m_random_generator;
#else
        mutable std::minstd_rand0 m_random_generator;
#endif

        typedef std::multimap<
            std::string,
            std::tuple<remote_t::key_type, resolve_result_type>
        > remote_service_map_t;

        remote_service_map_t m_remote_services;
};

} // namespace cocaine

#endif
