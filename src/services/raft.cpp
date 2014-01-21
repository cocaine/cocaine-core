/*
    Copyright (c) 2013-2013 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#include "cocaine/detail/services/raft.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include <iostream>

using namespace cocaine;
using namespace cocaine::service;

using namespace std::placeholders;

namespace cocaine { namespace io {

struct counter_tag;

struct counter {

    struct inc {
        typedef counter_tag tag;

        static
        const char*
        alias() {
            return "inc";
        }

        typedef boost::mpl::list<
            int
        > tuple_type;

        typedef void drain_type;
    };

    struct dec {
        typedef counter_tag tag;

        static
        const char*
        alias() {
            return "dec";
        }

        typedef boost::mpl::list<
            int
        > tuple_type;

        typedef void drain_type;
    };

    struct cas {
        typedef counter_tag tag;

        static
        const char*
        alias() {
            return "cas";
        }

        typedef boost::mpl::list<
            int,
            int
        > tuple_type;

        typedef void drain_type;
    };

}; // struct counter

template<>
struct protocol<counter_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        counter::inc,
        counter::dec,
        counter::cas
    > messages;

    typedef counter type;
};

}} // namespace cocaine::io

struct counter_t:
    public implements<io::counter_tag>
{
    typedef io::counter_tag tag;
    typedef int snapshot_type;

    counter_t(context_t& context, const std::string& name):
        implements<io::counter_tag>(context, name),
        m_log(new logging::log_t(context, name)),
        m_value(0)
    {
        COCAINE_LOG_DEBUG(m_log, "Initialize counter state machine %s.", name);

        on<io::counter::inc>(std::bind(&counter_t::inc, this, std::placeholders::_1));
        on<io::counter::dec>(std::bind(&counter_t::dec, this, std::placeholders::_1));
        on<io::counter::cas>(std::bind(&counter_t::cas, this, std::placeholders::_1, std::placeholders::_2));
    }

    virtual
    auto
    prototype() -> dispatch_t& {
        return *this;
    }

    snapshot_type
    snapshot() const {
        return m_value;
    }

    void
    consume(const snapshot_type& snapshot) {
        m_value = snapshot;
    }

    void
    inc(int n) {
        m_value += n;
        COCAINE_LOG_DEBUG(m_log, "Add value to counter: %d. Value is %d.", n, m_value);
    }

    void
    dec(int n) {
        m_value -= n;
        COCAINE_LOG_DEBUG(m_log, "Subtract value from counter: %d. Value is %d.", n, m_value);
    }

    void
    cas(int expected, int desired) {
        m_value.compare_exchange_strong(expected, desired);
        COCAINE_LOG_DEBUG(m_log, "Compare and swap: %d, %d. Value is %d.", expected, desired, m_value);
    }

private:
    const std::unique_ptr<logging::log_t> m_log;

    std::atomic<int> m_value;
};

raft_t::raft_t(context_t& context,
               io::reactor_t& reactor,
               const std::string& name,
               const dynamic_t& args):
    api::service_t(context, reactor, name, args),
    implements<io::raft_tag<msgpack::object, msgpack::object>>(context, name),
    m_context(context),
    m_reactor(reactor),
    m_log(new logging::log_t(context, name)),
    m_test_timer(reactor.native()),
    m_self(context.config.network.endpoint, context.config.network.locator)
{
    COCAINE_LOG_DEBUG(m_log, "Starting RAFT service with name %s.", name);

    m_cluster = args.as_object().at("cluster", dynamic_t::empty_object).to<raft::cluster_t>();
    m_election_timeout = args.as_object().at("election-timeout", 1500).to<unsigned int>();
    m_heartbeat_timeout = args.as_object().at("heartbeat-timeout", m_election_timeout / 2).to<unsigned int>();

    using namespace std::placeholders;

    on<io::raft<msgpack::object, msgpack::object>::append>(std::bind(&raft_t::append, this, _1, _2, _3, _4, _5, _6));
    on<io::raft<msgpack::object, msgpack::object>::apply>(std::bind(&raft_t::apply, this, _1, _2, _3, _4, _5, _6));
    on<io::raft<msgpack::object, msgpack::object>::request_vote>(std::bind(&raft_t::request_vote, this, _1, _2, _3, _4));

    // TODO: Remove this code after testing.
    COCAINE_LOG_DEBUG(m_log, "Adding 'counter' state machine to the RAFT.");

    this->add("counter", std::make_shared<counter_t>(context, "counter"));

    m_test_timer.set<raft_t, &raft_t::do_something>(this);
    m_test_timer.start(10.0, 10.0);
}

deferred<std::tuple<uint64_t, bool>>
raft_t::append(const std::string& state_machine,
               uint64_t term,
               raft::node_id_t leader,
               std::tuple<uint64_t, uint64_t> prev_entry, // index, term
               const std::vector<msgpack::object>& entries,
               uint64_t commit_index)
{
    auto actors = m_actors.synchronize();

    auto it = actors->find(state_machine);

    if(it != actors->end()) {
        return it->second->append(term, leader, prev_entry, entries, commit_index);
    } else {
        throw error_t("There is no such state machine.");
    }
}

deferred<std::tuple<uint64_t, bool>>
raft_t::apply(const std::string& state_machine,
              uint64_t term,
              raft::node_id_t leader,
              std::tuple<uint64_t, uint64_t> snapshot_entry, // index, term
              const msgpack::object& snapshot,
              uint64_t commit_index)
{
    auto actors = m_actors.synchronize();

    auto it = actors->find(state_machine);

    if(it != actors->end()) {
        return it->second->apply(term, leader, snapshot_entry, snapshot, commit_index);
    } else {
        throw error_t("There is no such state machine.");
    }
}

deferred<std::tuple<uint64_t, bool>>
raft_t::request_vote(const std::string& state_machine,
                     uint64_t term,
                     raft::node_id_t candidate,
                     std::tuple<uint64_t, uint64_t> last_entry)
{
    auto actors = m_actors.synchronize();

    auto it = actors->find(state_machine);

    if(it != actors->end()) {
        return it->second->request_vote(term, candidate, last_entry);
    } else {
        throw error_t("There is no such state machine.");
    }
}

namespace {
    void
    commit_result2(boost::optional<uint64_t> index) {
        if(index) {
            std::cerr << "An entry committed with index " << *index << std::endl;
        } else {
            std::cerr << "Something wrong." << std::endl;
        }
    }
}

void
raft_t::do_something(ev::timer&, int) {
    auto actors = m_actors.synchronize();

    auto it = actors->find("counter");

    if(it != actors->end()) {
        typedef raft::actor<counter_t, raft::configuration<raft::log<counter_t>>> actor_type;

        actor_type *a = static_cast<actor_type*>(it->second.get());

        actor_type::entry_type::command_type cmd;

        if(rand() % 2 == 0) {
            cmd = io::aux::frozen<io::counter::dec>(io::counter::dec(), rand() % 50);
        } else {
            cmd = io::aux::frozen<io::counter::inc>(io::counter::inc(), rand() % 50);
        }

        if(!a->call(&commit_result2, cmd)) {
            COCAINE_LOG_DEBUG(m_log,
                              "I'm not leader. Leader is %s:%d.",
                              a->leader_id().first,
                              a->leader_id().second);
        }
    }
}
