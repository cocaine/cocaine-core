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

#include "cocaine/detail/group.hpp"
#include "cocaine/api/gateway.hpp"

#include <mutex>
#include <queue>
#include <random>

namespace ev {
    struct io;
    struct timer;
}

namespace cocaine {

namespace detail {

#if defined(__clang__) || defined(HAVE_GCC46)
    typedef std::default_random_engine random_generator_t;
#else
    typedef std::minstd_rand0 random_generator_t;
#endif

} // namespace detail

class locator_t;

class group_index_t {
    public:
        group_index_t();

        group_index_t(const std::map<std::string, unsigned int>& group);

        void
        add(size_t service_index);

        void
        remove(size_t service_index);

        const std::vector<std::string>&
        services() const {
            return m_services;
        }

        const std::vector<unsigned int>&
        weights() const {
            return m_weights;
        }

        const std::vector<unsigned int>&
        used_weights() const {
            return m_used_weights;
        }

        unsigned int
        sum() const {
            return m_sum;
        }

    private:
        std::vector<std::string> m_services;
        std::vector<unsigned int> m_weights;
        std::vector<unsigned int> m_used_weights; // = original weight or 0 if there is no such service in locator
        unsigned int m_sum;
};

class groups_t {
    public:
        groups_t(locator_t &locator);

        void
        add_group(const std::string& name, const std::map<std::string, unsigned int>& group);

        void
        remove_group(const std::string& name);

        void
        add_service(const std::string& name);

        void
        remove_service(const std::string& name);

        std::string
        select_service(const std::string& group_name) const;

    private:
        typedef std::map<std::string, std::map<std::string, size_t>> // {service: {group: index in services vector}}
                inverted_index_t;

        std::map<std::string, group_index_t> m_groups; // index group -> services
        inverted_index_t m_inverted; // inverted for m_groups index service -> groups

        locator_t &m_locator;

        mutable detail::random_generator_t m_generator;
};

class services_t {
    public:
        typedef std::vector<std::pair<std::string, api::resolve_result_type>>
                services_vector_t;

    public:
        services_t(locator_t &locator);

        void // added
        add_local(const std::string& name);

        void // removed
        remove_local(const std::string& name);

        std::pair<services_vector_t, services_vector_t> // added, removed
        update_remote(const std::string& uuid, const api::synchronize_result_type& dump);

        std::map<std::string, api::resolve_result_type> // services of removed node
        remove_remote(const std::string& uuid);

        bool
        has(const std::string& name) const;

        void
        add_group(const std::string& name, const std::map<std::string, unsigned int>& group);

        void
        remove_group(const std::string& name);

        // Takes name of service or group and returns name of service.
        std::string
        select_service(const std::string& name) const;

    private:
        void
        add(const std::string& uuid, const std::string& name, const api::resolve_result_type& info);

        void
        remove(const std::string& uuid, const std::string& name);

    private:
        typedef std::map<std::string, std::map<std::string, api::resolve_result_type>>
                inverted_index_t;

        // service -> uuid's of remote nodes, that have this service
        std::map<std::string, std::set<std::string>> m_services;
        // Inverted index.
        inverted_index_t m_inverted; // uuid -> {(service, info)}

        // Groups index.
        groups_t m_groups;
};

class actor_t;

typedef io::event_traits<io::locator::resolve>::result_type resolve_result_type;
typedef io::event_traits<io::locator::synchronize>::result_type synchronize_result_type;

class locator_t:
    public dispatch_t
{
    typedef std::tuple<std::string, std::string, uint16_t> key_type;

    public:
        locator_t(context_t& context, io::reactor_t& reactor);

        virtual
       ~locator_t();

        void
        connect();

        void
        disconnect();

        void
        attach(const std::string& name, std::unique_ptr<actor_t>&& service);

        auto
        detach(const std::string& name) -> std::unique_ptr<actor_t>;

        const std::unique_ptr<logging::log_t>&
        logger() const;

        bool
        has_service(const std::string& name) const;

    private:
        resolve_result_type
        query(const std::unique_ptr<actor_t>& service) const;

        resolve_result_type
        resolve(const std::string& name) const;

        synchronize_result_type
        dump() const;

        void
        remove_uuid(const std::string& uuid);

        // Cluster I/O

        void
        on_announce_event(ev::io&, int);

        void
        on_announce_timer(ev::timer&, int);

        void
        on_message(const key_type& key, const io::message_t& message);

        void
        on_failure(const key_type& key, const std::error_code& ec);

        void
        on_timeout(const key_type& key);

    private:
        context_t& m_context;
        const std::unique_ptr<logging::log_t> m_log;

        // For future cluster locator interconnections.
        io::reactor_t& m_reactor;

        // Ports available for allocation.
        std::priority_queue<uint16_t, std::vector<uint16_t>, std::greater<uint16_t>> m_ports;

        // Contains services and groups which are present in the locator.
        services_t m_services_index;

        typedef std::vector<
            std::pair<std::string, std::unique_ptr<actor_t>>
        > service_list_t;

        // These are the instances of all the configured services, stored as a vector of pairs to
        // preserve the initialization order.
        service_list_t m_services;

        // As, for example, the Node Service can manipulate service list from its actor thread, the
        // service list access should be synchronized.
        mutable std::mutex m_services_mutex;

        // Announce receiver.
        std::unique_ptr<io::socket<io::udp>> m_sink;
        std::unique_ptr<ev::io> m_sink_watcher;

        // Remote gateway.
        std::unique_ptr<api::gateway_t> m_gateway;

        struct remote_t {
            std::shared_ptr<io::channel<io::socket<io::tcp>>> channel;
            std::shared_ptr<io::timeout_t> timeout;
        };

        // These are remote channels indexed by endpoint and uuid. The uuid is required to easily
        // disambiguate between different runtime instances on the same host.
        std::map<key_type, remote_t> m_remotes;

        // Announce emitter.
        std::unique_ptr<io::socket<io::udp>> m_announce;
        std::unique_ptr<ev::timer> m_announce_timer;

        struct synchronize_slot_t;

        // Synchronizing slot.
        std::shared_ptr<synchronize_slot_t> m_synchronizer;
};

} // namespace cocaine

#endif
