/*
    Copyright (c) 2014-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_RAFT_CONFIGURATION_MACHINE_HPP
#define COCAINE_RAFT_CONFIGURATION_MACHINE_HPP

#include "cocaine/detail/raft/repository.hpp"
#include "cocaine/detail/raft/control_service.hpp"
#include "cocaine/detail/raft/client.hpp"
#include "cocaine/traits/raft.hpp"
#include "cocaine/traits/map.hpp"
#include "cocaine/context.hpp"

namespace cocaine {

namespace raft {

struct configuration_machine_tag;

struct configuration_machine {

    // Add node to a cluster. If the node is the first node in the cluster, it's allowed to create
    // cluster by adding entry to its log and become leader
    // (without joining the cluster via the standard procedure).
    struct insert {
        typedef configuration_machine_tag tag;

        typedef boost::mpl::list<
            uint64_t,
            std::string,
            raft::node_id_t
        > tuple_type;

        typedef void result_type;
    };

    // Remove node from a cluster.
    struct erase {
        typedef configuration_machine_tag tag;

        typedef boost::mpl::list<
            uint64_t,
            std::string,
            raft::node_id_t
        > tuple_type;

        typedef void result_type;
    };

    // This is a service command, which is internally used by the configuration machine.
    // It's not exposed to user interface.
    struct commit {
        typedef configuration_machine_tag tag;

        typedef boost::mpl::list<
            std::string
        > tuple_type;

        typedef void result_type;
    };

    // Following two commands are supposed to be used by a system administrator
    // as an extreme measure in case of some bugs, when the configuration machine stops to
    // make progress.
    // System administrator can lock a configuration, so new nodes will not be allowed to join
    // cluster. Then the administrator can shutdown all machines, which are already in the cluster
    // and clear the configuration with "reset" command.
    struct lock {
        typedef configuration_machine_tag tag;

        typedef boost::mpl::list<
            std::string
        > tuple_type;

        typedef void result_type;
    };

    struct reset {
        typedef configuration_machine_tag tag;

        typedef boost::mpl::list<
            std::string,
            cluster_config_t
        > tuple_type;

        typedef void result_type;
    };

}; // struct configuration_machine

} // namespace raft

namespace io {

template<>
struct protocol<cocaine::raft::configuration_machine_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        cocaine::raft::configuration_machine::insert,
        cocaine::raft::configuration_machine::erase,
        cocaine::raft::configuration_machine::commit,
        cocaine::raft::configuration_machine::lock,
        cocaine::raft::configuration_machine::reset
    > messages;

    typedef cocaine::raft::configuration_machine type;
};

} // namespace io

namespace raft {

class configuration_machine_t {
public:
    typedef configuration_machine_tag tag;
    typedef repository_t::configs_type snapshot_type;
    typedef uint64_t operation_id_t;
    typedef std::function<void(const boost::variant<std::error_code, cluster_change_result>&)>
            operation_callback_t;
    typedef std::queue<std::pair<operation_id_t, operation_callback_t>> operaitons_queue_t;

    configuration_machine_t(context_t &context, io::reactor_t &reactor, control_service_t &service):
        m_context(&context),
        m_reactor(&reactor),
        m_service(&service),
        m_log(new logging::log_t(context, "configuration_machine"))
    { }

    configuration_machine_t(configuration_machine_t&& other) {
        *this = std::move(other);
    }

    configuration_machine_t&
    operator=(configuration_machine_t&& other) {
        m_context = other.m_context;
        m_reactor = other.m_reactor;
        m_service = other.m_service;
        m_log = std::move(other.m_log);

        return *this;
    }

    snapshot_type
    snapshot() const {
        return *m_context->raft().m_configs.synchronize();
    }

    void
    consume(const snapshot_type& snapshot) {
        *m_context->raft().m_configs.synchronize() = snapshot;
        update_appliers();
    }

    void
    begin_leadership() {
        m_is_leader = true;
    }

    void
    finish_leadership() {
        m_is_leader = false;
        m_appliers.clear();
    }

    void
    complete_log() {
        if(m_is_leader) {
            // Here we can safely apply changes to clusters.
            for(auto it = m_modified.begin(); it != m_modified.end(); ++it) {
                apply_config(*it);
            }
        }
    }

    void
    operator()(const io::aux::frozen<configuration_machine::insert>& req) {
        operation_id_t op_id;
        std::string machine_name;
        node_id_t node;
        std::tie(op_id, machine_name, node) = req.tuple;

        auto callback = pop_operation(machine_name, op_id);

        COCAINE_LOG_DEBUG(m_log,
                          "insert new node to %s's configuration: %s:%d",
                          machine_name,
                          node.first,
                          node.second)
        (blackhole::attribute::list({
            {"machine_name", machine_name},
            {"node_host", node.first},
            {"node_port", node.second}
        }));

        auto configs = m_context->raft().m_configs.synchronize();

        auto &config = configs->insert(
            std::make_pair(machine_name, lockable_config_t {false, cluster_config_t()})
        ).first->second;

        if(!config.locked) {
            bool already_in_cluster = (config.cluster.current.count(node) > 0) &&
                                      (!config.cluster.transitional() ||
                                       (config.cluster.next->count(node) > 0));

            if(already_in_cluster) {
                auto change_result = (config.cluster.current.size() == 1) ?
                                     cluster_change_result::new_cluster :
                                     cluster_change_result::done;

                if(callback) {
                    callback(change_result);
                }
                return;
            } else if(!config.cluster.transitional()) {
                config.cluster.insert(node);

                if(config.cluster.next->size() == 1) {
                    config.cluster.commit();
                    if(callback) {
                        callback(cluster_change_result::new_cluster);
                    }
                } else {
                    m_modified.insert(machine_name);
                    if(callback) {
                        m_active_operations[machine_name] = callback;
                    }
                }
                return;
            }
        }

        if(callback) {
            callback(std::error_code(raft_errc::busy));
        }
    }

    void
    operator()(const io::aux::frozen<configuration_machine::erase>& req) {
        operation_id_t op_id;
        std::string machine_name;
        node_id_t node;
        std::tie(op_id, machine_name, node) = req.tuple;

        auto callback = pop_operation(machine_name, op_id);

        COCAINE_LOG_DEBUG(m_log,
                          "erase node from %s's configuration: %s:%d",
                          machine_name,
                          node.first,
                          node.second)
        (blackhole::attribute::list({
            {"machine_name", machine_name},
            {"node_host", node.first},
            {"node_port", node.second}
        }));

        auto configs = m_context->raft().m_configs.synchronize();

        auto map_pair = configs->find(machine_name);

        if(map_pair == configs->end()) {
            if(callback) {
                callback(cluster_change_result::done);
            }
            return;
        }

        auto &config = map_pair->second;

        if(!config.locked) {
            bool already_removed = (config.cluster.current.count(node) == 0) &&
                                   (config.cluster.next->count(node) == 0);

            if(already_removed) {
                if(callback) {
                    callback(cluster_change_result::done);
                }
                return;
            } else if(!config.cluster.transitional()) {
                config.cluster.erase(node);

                m_modified.insert(machine_name);

                if(callback) {
                    m_active_operations[machine_name] = callback;
                }

                return;
            }
        }

        if(callback) {
            callback(std::error_code(raft_errc::busy));
        }
    }

    void
    operator()(const io::aux::frozen<configuration_machine::commit>& req) {
        std::string machine_name;
        std::tie(machine_name) = req.tuple;

        COCAINE_LOG_DEBUG(m_log, "commit %s's changes", machine_name);

        {
            auto configs = m_context->raft().m_configs.synchronize();

            auto map_pair = configs->find(machine_name);

            if(map_pair == configs->end()) {
                COCAINE_LOG_WARNING(m_log,
                                    "commit message for unknown state machine received: %s",
                                    machine_name)
                (blackhole::attribute::list({
                    {"machine_name", machine_name}
                }));
                return;
            }

            auto &config = map_pair->second;

            if(config.cluster.transitional()) {
                config.cluster.commit();
            }

            if(config.cluster.current.empty()) {
                configs->erase(map_pair);
            }
        }

        m_appliers.erase(machine_name);
        m_modified.erase(machine_name);

        auto operation_iter = m_active_operations.find(machine_name);

        if(operation_iter != m_active_operations.end()) {
            operation_iter->second(cluster_change_result::done);
        }
    }

    void
    operator()(const io::aux::frozen<configuration_machine::lock>& req) {
        std::string machine_name;
        std::tie(machine_name) = req.tuple;

        auto configs = m_context->raft().m_configs.synchronize();

        auto map_pair = configs->find(machine_name);

        if(map_pair != configs->end()) {
            map_pair->second.locked = true;
        }
    }

    void
    operator()(const io::aux::frozen<configuration_machine::reset>& req) {
        std::string machine_name;
        cluster_config_t new_value;
        std::tie(machine_name, new_value) = req.tuple;

        COCAINE_LOG_DEBUG(m_log, "reset %s's configuration", machine_name)
        (blackhole::attribute::list({
            {"machine_name", machine_name}
        }));

        if((new_value.transitional() && new_value.next->empty()) || new_value.current.empty()) {
            m_context->raft().m_configs.synchronize()->erase(machine_name);
            m_modified.erase(machine_name);
            m_appliers.erase(machine_name);
        } else {
            (*m_context->raft().m_configs.synchronize())[machine_name] = lockable_config_t {
                false,
                new_value
            };
        }

        update_appliers();
    }

    operation_id_t
    push_operation(const std::string& machine_name, const operation_callback_t& callback) {
        auto operation_id = m_operations_counter++;

        auto operations = m_operations.synchronize();

        auto machine_iter = operations->insert(std::make_pair(
            machine_name,
            std::queue<std::pair<operation_id_t, operation_callback_t>>()
        )).first;

        auto &queue = machine_iter->second;

        queue.push(std::make_pair(operation_id, callback));

        return operation_id;
    }

    operation_callback_t
    pop_operation(const std::string& machine_name, operation_id_t id) {
        auto operations = m_operations.synchronize();

        auto machine_iter = operations->find(machine_name);

        if(machine_iter == operations->end()) {
            return nullptr;
        }

        auto &queue = machine_iter->second;

        operation_callback_t result;

        while(!queue.empty() && queue.front().first <= id) {
            if(queue.front().first == id) {
                result = queue.front().second;
            }

            queue.pop();
        }

        return result;
    }

private:
    void
    update_appliers() {
        m_modified.clear();
        m_appliers.clear();

        auto configs = m_context->raft().m_configs.synchronize();

        for(auto it = configs->begin(); it != configs->end(); ++it) {
            const auto &name = it->first;
            const auto &config = it->second;

            if(config.cluster.transitional()) {
                m_modified.insert(name);
            }
        }
    }

    void
    apply_config(const std::string& name) {
        std::shared_ptr<disposable_client_t> *applier;

        std::vector<node_id_t> intersection;
        std::vector<node_id_t> added;
        std::vector<node_id_t> removed;

        {
            auto configs = m_context->raft().m_configs.synchronize();
            auto map_pair = configs->find(name);

            if(map_pair != configs->end()) {
                auto &config = map_pair->second;

                if(config.cluster.transitional()) {
                    auto insertion_pair = m_appliers.insert(
                        std::make_pair(name, std::shared_ptr<disposable_client_t>())
                    );

                    // New applier was added to map.
                    if(insertion_pair.second) {
                        std::set_intersection(config.cluster.current.begin(),
                                              config.cluster.current.end(),
                                              config.cluster.next->begin(),
                                              config.cluster.next->end(),
                                              std::back_inserter(intersection));

                        std::set_difference(config.cluster.next->begin(),
                                            config.cluster.next->end(),
                                            intersection.begin(),
                                            intersection.end(),
                                            std::back_inserter(added));

                        std::set_difference(config.cluster.current.begin(),
                                            config.cluster.current.end(),
                                            intersection.begin(),
                                            intersection.end(),
                                            std::back_inserter(removed));

                        applier = &insertion_pair.first->second;
                    } else {
                        // Some applier already works. Just do nothing.
                        return;
                    }
                } else {
                    m_modified.erase(name);
                    m_appliers.erase(name);
                    return;
                }
            } else {
                m_modified.erase(name);
                m_appliers.erase(name);
                return;
            }
        }

        auto local_actor = m_context->raft().get(name);

        if(local_actor) {
            auto leader = local_actor->leader_id();
            if(leader != node_id_t()) {
                intersection.insert(intersection.begin(), leader);
            }
        }

        *applier = std::make_shared<disposable_client_t>(
            *m_context,
            *m_reactor,
            m_context->raft().options().node_service_name,
            intersection
        );

        using namespace std::placeholders;

        auto error_handler = std::bind(&configuration_machine_t::reapply_config,
                                       this,
                                       name);

        auto result_handler = std::bind(&configuration_machine_t::commit_config,
                                        this,
                                        name,
                                        _1);

        typedef io::raft_node<msgpack::object, msgpack::object> protocol;

        if(!added.empty()) {
            (*applier)->call<protocol::insert>(result_handler,
                                               error_handler,
                                               name,
                                               added.front());
        } else if(!removed.empty()) {
            (*applier)->call<protocol::erase>(result_handler,
                                              error_handler,
                                              name,
                                              removed.front());
        }
    }

    void
    commit_config(const std::string& name, const command_result<void>& result) {
        // Disable applier, but don't remove the configuration from m_modified.
        // If "commit" entry will not be committed, the machine will try to apply
        // the configuration again.
        m_appliers.erase(name);

        if(m_is_leader && !result.error()) {
            m_service->configuration_actor()->call<configuration_machine::commit>(nullptr, name);
        }
    }

    void
    reapply_config(const std::string& name) {
        m_appliers.erase(name);
        apply_config(name);
    }

private:
    context_t *m_context;

    io::reactor_t *m_reactor;

    control_service_t *m_service;

    std::unique_ptr<logging::log_t> m_log;

    bool m_is_leader;

    std::set<std::string> m_modified;

    std::map<std::string, std::shared_ptr<disposable_client_t>> m_appliers;

    std::atomic<operation_id_t> m_operations_counter;

    synchronized<std::map<std::string, operaitons_queue_t>> m_operations;

    std::map<std::string, operation_callback_t> m_active_operations;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_CONFIGURATION_MACHINE_HPP
