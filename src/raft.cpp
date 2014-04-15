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

#include "cocaine/detail/raft/node_service.hpp"
#include "cocaine/detail/raft/control_service.hpp"
#include "cocaine/detail/raft/configuration_machine.hpp"
#include "cocaine/detail/raft/entry.hpp"
#include "cocaine/detail/raft/repository.hpp"

#include "cocaine/logging.hpp"

#include "cocaine/traits/vector.hpp"

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
        return std::shared_ptr<raft::actor_concept_t>();
    }
}

node_service_t::node_service_t(context_t& context, io::reactor_t& reactor, const std::string& name):
    api::service_t(context, reactor, name, dynamic_t::empty_object),
    implements<io::raft_node_tag<msgpack::object, msgpack::object>>(name),
    m_context(context),
    m_log(new logging::log_t(context, name))
{
    using namespace std::placeholders;

    typedef io::raft_node<msgpack::object, msgpack::object> protocol_type;

    on<protocol_type::append>(std::bind(&node_service_t::append, this, _1, _2, _3, _4, _5, _6));
    on<protocol_type::apply>(std::bind(&node_service_t::apply, this, _1, _2, _3, _4, _5, _6));
    on<protocol_type::request_vote>(std::bind(&node_service_t::request_vote, this, _1, _2, _3, _4));
    on<protocol_type::insert>(std::bind(&node_service_t::insert, this, _1, _2));
    on<protocol_type::erase>(std::bind(&node_service_t::erase, this, _1, _2));
}

std::shared_ptr<raft::actor_concept_t>
node_service_t::find_machine(const std::string& name) const {
    auto machine = m_context.raft->get(name);

    if(machine) {
        return machine;
    } else {
        throw error_t("There is no such state machine.");
    }
}

deferred<std::tuple<uint64_t, bool>>
node_service_t::append(const std::string& machine,
                       uint64_t term,
                       node_id_t leader,
                       std::tuple<uint64_t, uint64_t> prev_entry, // index, term
                       const std::vector<msgpack::object>& entries,
                       uint64_t commit_index)
{
    return find_machine(machine)->append(term, leader, prev_entry, entries, commit_index);
}

deferred<std::tuple<uint64_t, bool>>
node_service_t::apply(const std::string& machine,
                      uint64_t term,
                      raft::node_id_t leader,
                      std::tuple<uint64_t, uint64_t> snapshot_entry, // index, term
                      const msgpack::object& snapshot,
                      uint64_t commit_index)
{
    return find_machine(machine)->apply(term, leader, snapshot_entry, snapshot, commit_index);
}

deferred<std::tuple<uint64_t, bool>>
node_service_t::request_vote(const std::string& state_machine,
                             uint64_t term,
                             raft::node_id_t candidate,
                             std::tuple<uint64_t, uint64_t> last_entry)
{
    return find_machine(state_machine)->request_vote(term, candidate, last_entry);
}

deferred<command_result<void>>
node_service_t::insert(const std::string& machine, const raft::node_id_t& node) {
    return find_machine(machine)->insert(node);
}

deferred<command_result<void>>
node_service_t::erase(const std::string& machine, const raft::node_id_t& node) {
    return find_machine(machine)->erase(node);
}

control_service_t::control_service_t(context_t& context,
                                     io::reactor_t& reactor,
                                     const std::string& name):
    api::service_t(context, reactor, name, dynamic_t::empty_object),
    implements<io::raft_control_tag<msgpack::object, msgpack::object>>(name),
    m_context(context),
    m_reactor(reactor),
    m_log(new logging::log_t(context, name))
{
    using namespace std::placeholders;

    typedef io::raft_control<msgpack::object, msgpack::object> protocol_type;

    on<protocol_type::insert>(std::bind(&control_service_t::insert, this, _1, _2));
    on<protocol_type::erase>(std::bind(&control_service_t::erase, this, _1, _2));
    on<protocol_type::lock>(std::bind(&control_service_t::lock, this, _1));
    on<protocol_type::reset>(std::bind(&control_service_t::reset, this, _1, _2));
    on<protocol_type::dump>(std::bind(&control_service_t::dump, this));
    on<protocol_type::status>(std::bind(&control_service_t::status, this, _1));
    on<protocol_type::leader>(std::bind(&control_service_t::leader, this, _1));

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

const std::shared_ptr<control_service_t::config_actor_type>&
control_service_t::configuration_actor() const {
    return m_config_actor;
}

std::shared_ptr<raft::actor_concept_t>
control_service_t::find_machine(const std::string& name) const {
    auto machine = m_context.raft->get(name);

    if(machine) {
        return machine;
    } else {
        throw error_t("There is no such state machine.");
    }
}

deferred<command_result<cluster_change_result>>
control_service_t::insert(const std::string& machine, const raft::node_id_t& node) {
    deferred<command_result<cluster_change_result>> promise;

    using namespace std::placeholders;

    auto success_handler = std::bind(&control_service_t::on_config_change_result,
                                     this,
                                     promise,
                                     _1);

    auto op_id = m_config_actor->machine().push_operation(machine, success_handler);

    auto error_handler = std::bind(&control_service_t::on_config_change_error,
                                   this,
                                   machine,
                                   op_id,
                                   promise,
                                   _1);

    m_config_actor->call<configuration_machine::insert>(error_handler, op_id, machine, node);

    return promise;
}

deferred<command_result<cluster_change_result>>
control_service_t::erase(const std::string& machine, const raft::node_id_t& node) {
    deferred<command_result<cluster_change_result>> promise;

    using namespace std::placeholders;

    auto success_handler = std::bind(&control_service_t::on_config_change_result,
                                     this,
                                     promise,
                                     _1);

    auto op_id = m_config_actor->machine().push_operation(machine, success_handler);

    auto error_handler = std::bind(&control_service_t::on_config_change_error,
                                   this,
                                   machine,
                                   op_id,
                                   promise,
                                   _1);

    m_config_actor->call<configuration_machine::erase>(error_handler, op_id, machine, node);

    return promise;
}

deferred<command_result<void>>
control_service_t::lock(const std::string& machine) {
    deferred<command_result<void>> promise;

    m_config_actor->call<configuration_machine::lock>(
        std::bind(&control_service_t::on_config_void_result, this, promise, std::placeholders::_1),
        machine
    );

    return promise;
}

deferred<command_result<void>>
control_service_t::reset(const std::string& machine, const cluster_config_t& new_config) {
    deferred<command_result<void>> promise;

    m_config_actor->call<configuration_machine::reset>(
        std::bind(&control_service_t::on_config_void_result, this, promise, std::placeholders::_1),
        machine,
        new_config
    );

    return promise;
}

std::map<std::string, cocaine::raft::lockable_config_t>
control_service_t::dump() {
    return *m_context.raft->configuration();
}

actor_state
control_service_t::status(const std::string& machine) {
    return find_machine(machine)->status();
}

node_id_t
control_service_t::leader(const std::string& machine) {
    return find_machine(machine)->leader_id();
}

void
control_service_t::on_config_void_result(deferred<command_result<void>> promise,
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
control_service_t::on_config_change_error(const std::string& machine,
                                          uint64_t operation_id,
                                          deferred<command_result<cluster_change_result>> promise,
                                          const std::error_code& ec)
{
    if(ec) {
        m_config_actor->machine().pop_operation(machine, operation_id);

        auto errc = static_cast<raft_errc>(ec.value());
        promise.write(
            command_result<cluster_change_result>(errc, m_config_actor->leader_id())
        );
    }
}

void
control_service_t::on_config_change_result(
    deferred<command_result<cluster_change_result>> promise,
    const boost::variant<std::error_code, cluster_change_result>& result
) {
    if(boost::get<std::error_code>(&result)) {
        auto errc = static_cast<raft_errc>(boost::get<std::error_code>(result).value());
        promise.write(
            command_result<cluster_change_result>(errc, m_config_actor->leader_id())
        );
    } else {
        auto result_code = boost::get<cluster_change_result>(result);
        promise.write(command_result<cluster_change_result>(result_code));
    }
}

