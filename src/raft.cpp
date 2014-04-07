/*
    Copyright (c) 2013-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#include "cocaine/detail/raft/service.hpp"
#include "cocaine/detail/raft/configuration_machine.hpp"
#include "cocaine/detail/raft/entry.hpp"
#include "cocaine/raft.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::raft;

std::error_code
cocaine::make_error_code(raft_errc e) {
    return std::error_code(static_cast<int>(e), raft_category());
}

std::error_condition
cocaine::make_error_condition(raft_errc e) {
    return std::error_condition(static_cast<int>(e), raft_category());
}

raft::repository_t::repository_t(context_t& context):
    m_context(context),
    m_reactor(std::make_shared<io::reactor_t>()),
    m_id(m_context.config.network.hostname, m_context.config.network.locator)
{ }

std::shared_ptr<raft::actor_concept_t>
raft::repository_t::get(const std::string& name) const {
    auto actors = m_actors.synchronize();

    auto it = actors->find(name);

    if(it != actors->end()) {
        return it->second;
    } else {
        throw std::shared_ptr<raft::actor_concept_t>();
    }
}

#include <iostream>

service_t::service_t(context_t& context, io::reactor_t& reactor, const std::string& name):
    api::service_t(context, reactor, name, dynamic_t::empty_object),
    implements<io::raft_tag<msgpack::object, msgpack::object>>(context, name),
    m_context(context),
    m_reactor(reactor),
    m_log(new logging::log_t(context, name))
{
    using namespace std::placeholders;

    on<io::raft<msgpack::object, msgpack::object>::append>(std::bind(&service_t::append, this, _1, _2, _3, _4, _5, _6));
    on<io::raft<msgpack::object, msgpack::object>::apply>(std::bind(&service_t::apply, this, _1, _2, _3, _4, _5, _6));
    on<io::raft<msgpack::object, msgpack::object>::request_vote>(std::bind(&service_t::request_vote, this, _1, _2, _3, _4));
    on<io::raft<msgpack::object, msgpack::object>::insert_internal>(std::bind(&service_t::insert_internal, this, _1, _2));
    on<io::raft<msgpack::object, msgpack::object>::erase_internal>(std::bind(&service_t::erase_internal, this, _1, _2));
    on<io::raft<msgpack::object, msgpack::object>::insert>(std::bind(&service_t::insert, this, _1, _2));
    on<io::raft<msgpack::object, msgpack::object>::erase>(std::bind(&service_t::erase, this, _1, _2));
    on<io::raft<msgpack::object, msgpack::object>::lock>(std::bind(&service_t::lock, this, _1));
    on<io::raft<msgpack::object, msgpack::object>::reset>(std::bind(&service_t::reset, this, _1, _2));

    if(m_context.config.raft.create_configuration_cluster) {
        typedef log_entry<configuration_machine_t> entry_type;
        typedef configuration<configuration_machine_t> config_type;
        typedef log_traits<configuration_machine_t, config_type::cluster_type>::snapshot_type
                snapshot_type;

        config_type config(
            m_context.raft->id(),
            cluster_config_t {std::set<node_id_t>(), boost::none}
        );

        configuration_machine_t config_machine(m_context, m_reactor, *this);

        config.log().push(entry_type());
        config.log().push(entry_type());

        std::map<std::string, lockable_config_t> config_snapshot;

        config_snapshot[m_context.config.raft.config_machine_name] = lockable_config_t {
            false,
            cluster_config_t {
                std::set<node_id_t>({m_context.raft->id()}),
                boost::none
            }
        };

        config_machine.consume(config_snapshot);
        config.log().set_snapshot(1, 1, snapshot_type(std::move(config_snapshot), config.cluster()));

        config.set_current_term(1);
        config.set_commit_index(1);
        config.set_last_applied(1);

        m_config_actor = m_context.raft->insert(
            m_context.config.raft.config_machine_name,
            std::move(config_machine),
            std::move(config)
        );
    } else {
        m_config_actor = m_context.raft->insert(
            m_context.config.raft.config_machine_name,
            configuration_machine_t(m_context, m_reactor, *this)
        );
    }
}

const std::shared_ptr<service_t::config_actor_type>&
service_t::configuration_actor() const {
    return m_config_actor;
}

std::shared_ptr<raft::actor_concept_t>
service_t::find_machine(const std::string& name) const {
    auto machine = m_context.raft->get(name);

    if(machine) {
        return machine;
    } else {
        throw error_t("There is no such state machine.");
    }
}

deferred<std::tuple<uint64_t, bool>>
service_t::append(const std::string& machine,
                  uint64_t term,
                  node_id_t leader,
                  std::tuple<uint64_t, uint64_t> prev_entry, // index, term
                  const std::vector<msgpack::object>& entries,
                  uint64_t commit_index)
{
    return find_machine(machine)->append(term, leader, prev_entry, entries, commit_index);
}

deferred<std::tuple<uint64_t, bool>>
service_t::apply(const std::string& machine,
                 uint64_t term,
                 raft::node_id_t leader,
                 std::tuple<uint64_t, uint64_t> snapshot_entry, // index, term
                 const msgpack::object& snapshot,
                 uint64_t commit_index)
{
    return find_machine(machine)->apply(term, leader, snapshot_entry, snapshot, commit_index);
}

deferred<std::tuple<uint64_t, bool>>
service_t::request_vote(const std::string& state_machine,
                        uint64_t term,
                        raft::node_id_t candidate,
                        std::tuple<uint64_t, uint64_t> last_entry)
{
    return find_machine(state_machine)->request_vote(term, candidate, last_entry);
}

deferred<command_result<void>>
service_t::insert_internal(const std::string& machine, const raft::node_id_t& node) {
    return find_machine(machine)->insert(node);
}

deferred<command_result<void>>
service_t::erase_internal(const std::string& machine, const raft::node_id_t& node) {
    return find_machine(machine)->erase(node);
}

deferred<command_result<cluster_change_result>>
service_t::insert(const std::string& machine, const raft::node_id_t& node) {
    COCAINE_LOG_DEBUG(m_log,
                      "Insert request received: %s, %s:%d.",
                      machine,
                      node.first,
                      node.second);

    deferred<command_result<cluster_change_result>> promise;

    using namespace std::placeholders;

    auto success_handler = std::bind(&service_t::on_config_change_result, this, promise, _1);

    auto op_id = m_config_actor->machine().push_operation(machine, success_handler);

    auto error_handler = std::bind(&service_t::on_config_change_error,
                                   this,
                                   machine,
                                   op_id,
                                   promise,
                                   _1);

    m_config_actor->call<io::aux::frozen<configuration_machine::insert>>(
        error_handler,
        io::aux::make_frozen<configuration_machine::insert>(op_id, machine, node)
    );

    return promise;
}

deferred<command_result<cluster_change_result>>
service_t::erase(const std::string& machine, const raft::node_id_t& node) {
    COCAINE_LOG_DEBUG(m_log,
                      "Erase request received: %s, %s:%d.",
                      machine,
                      node.first,
                      node.second);

    deferred<command_result<cluster_change_result>> promise;

    using namespace std::placeholders;

    auto success_handler = std::bind(&service_t::on_config_change_result, this, promise, _1);

    auto op_id = m_config_actor->machine().push_operation(machine, success_handler);

    auto error_handler = std::bind(&service_t::on_config_change_error,
                                   this,
                                   machine,
                                   op_id,
                                   promise,
                                   _1);

    m_config_actor->call<io::aux::frozen<configuration_machine::erase>>(
        error_handler,
        io::aux::make_frozen<configuration_machine::erase>(op_id, machine, node)
    );

    return promise;
}

deferred<command_result<void>>
service_t::lock(const std::string& machine) {
    deferred<command_result<void>> promise;

    m_config_actor->call<io::aux::frozen<configuration_machine::lock>>(
        std::bind(&service_t::on_config_void_result, this, promise, std::placeholders::_1),
        io::aux::make_frozen<configuration_machine::lock>(machine)
    );

    return promise;
}

deferred<command_result<void>>
service_t::reset(const std::string& machine, const cluster_config_t& new_config) {
    COCAINE_LOG_DEBUG(m_log, "Reset request received: %s.", machine);

    deferred<command_result<void>> promise;

    m_config_actor->call<io::aux::frozen<configuration_machine::reset>>(
        std::bind(&service_t::on_config_void_result, this, promise, std::placeholders::_1),
        io::aux::make_frozen<configuration_machine::reset>(machine, new_config)
    );

    return promise;
}

void
service_t::on_config_void_result(deferred<command_result<void>> promise,
                                 const std::error_code& ec)
{
    if(ec) {
        promise.write(
            command_result<void>(static_cast<raft_errc>(ec.value()), m_config_actor->leader_id())
        );
    } else {
        promise.write(command_result<void>());
    }
}

void
service_t::on_config_change_error(const std::string& machine,
                                  uint64_t operation_id,
                                  deferred<command_result<cluster_change_result>> promise,
                                  const std::error_code& ec)
{
    COCAINE_LOG_DEBUG(m_log, "on_config_change_error: [%d] %s.", ec.value(), ec.message());
    if(ec) {
        // COCAINE_LOG_DEBUG(m_log, "on_config_change_error: Error code! : %s.", ec.message());
        m_config_actor->machine().pop_operation(machine, operation_id);

        auto errc = static_cast<raft_errc>(ec.value());
        promise.write(
            command_result<cluster_change_result>(errc, m_config_actor->leader_id())
        );
    }
}

void
service_t::on_config_change_result(
    deferred<command_result<cluster_change_result>> promise,
    const boost::variant<std::error_code, cluster_change_result>& result
) {
    COCAINE_LOG_DEBUG(m_log, "on_config_change_result");
    if(boost::get<std::error_code>(&result)) {
        COCAINE_LOG_DEBUG(m_log, "on_config_change_result: Error code! : %s.", boost::get<std::error_code>(result).message());
        auto errc = static_cast<raft_errc>(boost::get<std::error_code>(result).value());
        promise.write(
            command_result<cluster_change_result>(errc, m_config_actor->leader_id())
        );
    } else {
        COCAINE_LOG_DEBUG(m_log, "on_config_change_result: Okedoke");
        auto result_code = boost::get<cluster_change_result>(result);
        promise.write(command_result<cluster_change_result>(result_code));
    }
}

