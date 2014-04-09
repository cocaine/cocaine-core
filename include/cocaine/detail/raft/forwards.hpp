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

#ifndef COCAINE_RAFT_FORWARD_DECLARATIONS_HPP
#define COCAINE_RAFT_FORWARD_DECLARATIONS_HPP

#include "cocaine/detail/raft/error.hpp"

#include "cocaine/rpc/slots/deferred.hpp"

#include <set>
#include <string>
#include <utility>

namespace cocaine { namespace raft {

// Identifier of RAFT node. In fact this is endpoint of locator of the node.
typedef std::pair<std::string, uint16_t> node_id_t;

// Type of entire snapshot (with machine state and cluster configuration).
template<class StateMachine, class Cluster>
struct log_traits {
    typedef std::tuple<typename StateMachine::snapshot_type, Cluster> snapshot_type;
};

template<class T>
class command_result {
    template<class, class> friend struct cocaine::io::type_traits;

    typedef boost::variant<raft_errc, T> value_type;

public:
    explicit
    command_result(const T& value):
        m_value(value)
    { }

    explicit
    command_result(const raft_errc& errc = raft_errc::unknown,
                   const node_id_t& leader = node_id_t()):
        m_value(errc),
        m_leader(leader)
    { }

    std::error_code
    error() const {
        if(boost::get<raft_errc>(&m_value)) {
            return std::error_code(boost::get<raft_errc>(m_value));
        } else {
            return std::error_code();
        }
    }

    T&
    value() {
        return boost::get<T>(m_value);
    }

    const T&
    value() const {
        return boost::get<T>(m_value);
    }

    const node_id_t&
    leader() const {
        return m_leader;
    }

private:
    value_type m_value;
    node_id_t m_leader;
};

template<>
class command_result<void> {
    template<class, class> friend struct cocaine::io::type_traits;

    typedef boost::optional<raft_errc> value_type;

public:
    explicit
    command_result():
        m_value(boost::none)
    { }

    explicit
    command_result(const raft_errc& errc, const node_id_t& leader = node_id_t()):
        m_value(errc),
        m_leader(leader)
    { }

    std::error_code
    error() const {
        if(boost::get<raft_errc>(&m_value)) {
            return std::error_code(boost::get<raft_errc>(m_value));
        } else {
            return std::error_code();
        }
    }

    const node_id_t&
    leader() const {
        return m_leader;
    }

private:
    value_type m_value;
    node_id_t m_leader;
};

struct cluster_config_t {
    // Set of nodes in the cluster.
    std::set<node_id_t> current;

    // Set to be applied. If this field is set, then the configuration is transitional.
    boost::optional<std::set<node_id_t>> next;

    // Check if the configuration is transitional (see Raft paper).
    bool
    transitional() const {
        return next;
    }

    // Modifiers of configuration. These two methods move the configuration to transitional state.
    void
    insert(const node_id_t& node) {
        BOOST_ASSERT(!transitional());

        next = current;
        next->insert(node);
    }

    void
    erase(const node_id_t& node) {
        BOOST_ASSERT(!transitional());

        next = current;
        next->erase(node);
    }

    // Apply new set of nodes. This method moves the configuration from transitional state.
    void
    commit() {
        BOOST_ASSERT(next);

        current = std::move(*next);
        next = boost::none;
    }

    // If current configuration change fails, it should be undone.
    void
    rollback() {
        BOOST_ASSERT(next);

        next = boost::none;
    }
};

struct lockable_config_t {
    bool locked;
    cluster_config_t cluster;
};

enum class cluster_change_result {
    new_cluster,
    done
};

enum class actor_state {
    not_in_cluster,
    joined,
    recognized,
    follower,
    candidate,
    leader
};

// Concept of Raft actor, which should implement the algorithm.
// Here are defined methods to handle messages from leader and candidates. These methods must be thread-safe.
class actor_concept_t {
public:
    virtual
    deferred<std::tuple<uint64_t, bool>>
    append(uint64_t term,
           node_id_t leader,
           std::tuple<uint64_t, uint64_t> prev_entry, // index, term
           const std::vector<msgpack::object>& entries,
           uint64_t commit_index) = 0;

    virtual
    deferred<std::tuple<uint64_t, bool>>
    apply(uint64_t term,
          node_id_t leader,
          std::tuple<uint64_t, uint64_t> prev_entry, // index, term
          const msgpack::object& snapshot,
          uint64_t commit_index) = 0;

    virtual
    deferred<std::tuple<uint64_t, bool>>
    request_vote(uint64_t term,
                 node_id_t candidate,
                 std::tuple<uint64_t, uint64_t> last_entry) = 0;

    virtual
    deferred<command_result<void>>
    insert(const node_id_t& node) = 0;

    virtual
    deferred<command_result<void>>
    erase(const node_id_t& node) = 0;

    virtual
    node_id_t
    leader_id() const = 0;

    virtual
    actor_state
    status() const = 0;
};

template<class, class>
class actor;

template<class>
class configuration;

class configuration_machine_t;

namespace aux {

    // Use it instead of enable_if, when you want to check if a class has some method.
    // Usage: typename require_method<void(T::*)(int, int), &T::required_method>::type
    template<class T, T>
    struct require_method {
        typedef void type;
    };

}

}} // namespace cocaine::raft

#include "cocaine/detail/raft/options.hpp"
#include "cocaine/detail/raft/entry.hpp"

#endif // COCAINE_RAFT_FORWARD_DECLARATIONS_HPP
