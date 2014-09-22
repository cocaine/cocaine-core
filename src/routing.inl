/*
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

#include <random>

namespace routing {

#if defined(__clang__) || defined(HAVE_GCC46)
typedef std::default_random_engine random_generator_t;
typedef std::uniform_int_distribution<unsigned int> uniform_uint;
#else
typedef std::minstd_rand0 random_generator_t;
typedef std::uniform_int<unsigned int> uniform_uint;
#endif

struct group_index_t {
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
    std::vector<unsigned int> m_used_weights; // = original weight or 0 if there is no such service in the Locator
    unsigned int m_sum;
};

} // namespace routing

using namespace routing;

class locator_t::router_t {
    public:
        typedef std::vector<
            std::pair<std::string, locator_t::resolve_result_type>
        > services_vector_t;

    public:
        router_t(logging::log_t& log);

        void
        add_local(const std::string& name);

        void
        remove_local(const std::string& name);

        std::pair<services_vector_t, services_vector_t> // added, removed
        update_remote(const std::string& uuid, const synchronize_result_type& dump);

        std::map<std::string, resolve_result_type> // services of the removed node
        remove_remote(const std::string& uuid);

        bool
        has(const std::string& name) const;

        void
        update_group(const std::string& name, const std::map<std::string, unsigned int>& group);

        void
        remove_group(const std::string& name);

        std::string
        select_service(const std::string& name) const;

    private:
        void
        add(const std::string& uuid, const std::string& name, const resolve_result_type& info);

        void
        remove(const std::string& uuid, const std::string& name);

    private:
        typedef std::map<
            std::string,
            std::map<std::string, resolve_result_type>
        > inverted_index_t;

        // Service -> UUIDs of the remote nodes that have this service.
        std::map<std::string, std::set<std::string>> m_services;

        // Inverse index: UUID -> {(service, info)}.
        inverted_index_t m_inverted;

        struct groups_t {
            groups_t(logging::log_t& log, const locator_t::router_t& router);

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
            // Maps group name to services.
            std::map<std::string, group_index_t> m_groups;

            typedef std::map<
                std::string,
                std::map<std::string, size_t>
            > inverted_index_t;

            // Inverse index: {service: {group: index in services vector}}.
            inverted_index_t m_inverted;

            logging::log_t& m_log;
            const locator_t::router_t& m_router;

            mutable random_generator_t m_generator;
        };

        groups_t m_groups;

        // Router interlocking.
        mutable std::mutex m_mutex;
};

group_index_t::group_index_t() :
    m_sum(0)
{
    // Empty.
}

group_index_t::group_index_t(const std::map<std::string, unsigned int>& group) :
    m_used_weights(group.size(), 0),
    m_sum(0)
{
    for(auto it = group.begin(); it != group.end(); ++it) {
        m_services.push_back(it->first);
        m_weights.push_back(it->second);
    }
}

void
group_index_t::add(size_t service_index) {
    m_sum += m_weights[service_index];
    m_used_weights[service_index] = m_weights[service_index];
}

void
group_index_t::remove(size_t service_index) {
    m_sum -= m_weights[service_index];
    m_used_weights[service_index] = 0;
}

locator_t::router_t::groups_t::groups_t(logging::log_t& log, const router_t& router) :
    m_log(log),
    m_router(router)
{
#if defined(__clang__) || defined(HAVE_GCC46)
    std::random_device device;
    m_generator.seed(device());
#else
    m_generator.seed(static_cast<unsigned long>(::time(nullptr)));
#endif
}

void
locator_t::router_t::groups_t::add_group(const std::string& name, const std::map<std::string, unsigned int>& group) {
    COCAINE_LOG_INFO((&m_log), "adding group '%s'", name);

    m_groups[name] = group_index_t(group);
    auto group_it = m_groups.find(name);

    for(size_t i = 0; i < group.size(); ++i) {
        m_inverted[group_it->second.services()[i]][name] = i;

        if(m_router.has(group_it->second.services()[i])) {
            group_it->second.add(i);
        }
    }
}

void
locator_t::router_t::groups_t::remove_group(const std::string& name) {
    auto group_it = m_groups.find(name);

    if(group_it == m_groups.end()) {
        return;
    }

    const auto& services = group_it->second.services();

    for(size_t i = 0; i < services.size(); ++i) {
        auto service_it = m_inverted.find(services[i]);

        if(service_it != m_inverted.end()) {
            service_it->second.erase(name);
            if(service_it->second.empty()) {
                m_inverted.erase(service_it);
            }
        }
    }

    m_groups.erase(group_it);

    COCAINE_LOG_INFO((&m_log), "group '%s' has been removed", name);
}

void
locator_t::router_t::groups_t::add_service(const std::string& name) {
    auto service_it = m_inverted.find(name);

    if(service_it == m_inverted.end()) {
        return;
    }

    for(auto it = service_it->second.begin(); it != service_it->second.end(); ++it) {
        m_groups[it->first].add(it->second);
    }
}

void
locator_t::router_t::groups_t::remove_service(const std::string& name) {
    auto service_it = m_inverted.find(name);

    if(service_it == m_inverted.end()) {
        return;
    }

    for(auto it = service_it->second.begin(); it != service_it->second.end(); ++it) {
        m_groups[it->first].remove(it->second);
    }
}

std::string
locator_t::router_t::groups_t::select_service(const std::string& group_name) const {
    auto group_it = m_groups.find(group_name);

    if(group_it == m_groups.end() || group_it->second.sum() == 0) {
        return group_name;
    }

    uniform_uint distribution(1, group_it->second.sum());
    unsigned int max = distribution(m_generator);

    for(size_t i = 0; i < group_it->second.services().size(); ++i) {
        if(max <= group_it->second.used_weights()[i]) {
            return group_it->second.services()[i];
        } else {
            max -= group_it->second.used_weights()[i];
        }
    }

    return group_name;
}

locator_t::router_t::router_t(logging::log_t& log):
    m_groups(log, *this)
{
    // Empty.
}

void
locator_t::router_t::add_local(const std::string& name) {
    std::lock_guard<std::mutex> guard(m_mutex);

    // "local" is a special "uuid" that indicates local services.
    add("local", name, resolve_result_type());
}

void
locator_t::router_t::remove_local(const std::string& name) {
    std::lock_guard<std::mutex> guard(m_mutex);

    // "local" is a special "uuid" that indicates local services.
    remove("local", name);
}

auto
locator_t::router_t::update_remote(const std::string& uuid, const synchronize_result_type& dump)
    -> std::pair<services_vector_t, services_vector_t>
{
    services_vector_t added, removed;
    std::lock_guard<std::mutex> guard(m_mutex);

    auto uuid_it = m_inverted.find(uuid);

    if(uuid_it != m_inverted.end()) {
        std::set_difference(
            uuid_it->second.begin(),
            uuid_it->second.end(),
            dump.begin(),
            dump.end(),
            std::back_inserter(removed),
            dump.value_comp()
        );

        std::set_difference(
            dump.begin(),
            dump.end(),
            uuid_it->second.begin(),
            uuid_it->second.end(),
            std::back_inserter(added),
            dump.value_comp()
        );

        for(auto it = removed.begin(); it != removed.end(); ++it) {
            remove(uuid, it->first);
        }

        for(auto it = added.begin(); it != added.end(); ++it) {
            add(uuid, it->first, it->second);
        }
    } else {
        added.assign(dump.begin(), dump.end());

        for(auto it = added.begin(); it != added.end(); ++it) {
            add(uuid, it->first, it->second);
        }
    }

    return std::make_pair(std::move(added), std::move(removed));
}

auto
locator_t::router_t::remove_remote(const std::string& uuid)
    -> std::map<std::string, resolve_result_type>
{
    std::map<std::string, resolve_result_type> removed;
    std::lock_guard<std::mutex> guard(m_mutex);

    auto uuid_it = m_inverted.find(uuid);

    if(uuid_it == m_inverted.end()) {
        return removed;
    }

    for(auto it = uuid_it->second.begin(); it != uuid_it->second.end(); ++it) {
        auto service_it = m_services.find(it->first);

        if(service_it != m_services.end()) {
            service_it->second.erase(uuid);

            if(service_it->second.empty()) {
                m_services.erase(service_it);
                m_groups.remove_service(it->first);
            }
        }
    }

    removed = std::move(uuid_it->second);

    m_inverted.erase(uuid_it);

    return removed;
}

bool
locator_t::router_t::has(const std::string& name) const {
    return m_services.find(name) != m_services.end();
}

void
locator_t::router_t::update_group(const std::string& name, const std::map<std::string, unsigned int>& group) {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_groups.remove_group(name);
    m_groups.add_group(name, group);
}

void
locator_t::router_t::remove_group(const std::string& name) {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_groups.remove_group(name);
}

std::string
locator_t::router_t::select_service(const std::string& name) const {
    std::lock_guard<std::mutex> guard(m_mutex);
    return m_groups.select_service(name);
}

void
locator_t::router_t::add(const std::string& uuid, const std::string& name, const resolve_result_type& info) {
    auto insert_result = m_services.insert(std::make_pair(name, std::set<std::string>()));
    insert_result.first->second.insert(uuid);
    m_inverted[uuid][name] = info;

    if(insert_result.second) {
        m_groups.add_service(name);
    }
}

void
locator_t::router_t::remove(const std::string& uuid, const std::string& name) {
    auto uuid_it = m_inverted.find(uuid);

    if(uuid_it != m_inverted.end()) {
        uuid_it->second.erase(name);

        if(uuid_it->second.empty()) {
            m_inverted.erase(uuid_it);
        }
    }

    auto service_it = m_services.find(name);

    if(service_it != m_services.end()) {
        service_it->second.erase(uuid);

        if(service_it->second.empty()) {
            m_services.erase(service_it);
            m_groups.remove_service(name);
        }
    }
}
