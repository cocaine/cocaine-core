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

using namespace cocaine;

using namespace std::placeholders;

namespace cocaine { namespace io {

// Storage service interface

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

    counter_t(context_t& context, const std::string& name):
        implements<io::counter_tag>(context, name),
        m_value(0)
    {
        on<io::counter::inc>(std::bind(&counter_t::inc, this, std::placeholders::_1));
        on<io::counter::dec>(std::bind(&counter_t::dec, this, std::placeholders::_1));
        on<io::counter::cas>(std::bind(&counter_t::cas, this, std::placeholders::_1, std::placeholders::_2));
    }

    virtual
    auto
    prototype() -> dispatch_t& {
        return *this;
    }

    void
    inc(int n) {
        m_value += n;
    }

    void
    dec(int n) {
        m_value -= n;
    }

    void
    cas(int expected, int desired) {
        m_value.compare_exchange_strong(expected, desired);
    }

private:
    std::atomic<int> m_value;
};

raft_service_t::raft_service_t(context_t& context,
                               io::reactor_t& reactor,
                               const std::string& name,
                               const dynamic_t& args):
    api::service_t(context, reactor, name, args),
    implements<io::raft_tag<msgpack::object>>(context, name),
    m_self(context.config.network.endpoint, context.config.network.locator),
    m_context(context),
    m_reactor(reactor)
{
    m_cluster = args.as_object().at("cluster", dynamic_t::empty_object).to<std::set<raft::node_id_t>>();
    m_election_timeout = args.as_object().at("election-timeout", 2500).to<uint64_t>();
    m_heartbeat_timeout = args.as_object().at("heartbeat-timeout", m_election_timeout / 2).to<uint64_t>();

    using namespace std::placeholders;

    on<io::raft<msgpack::object>::append>(std::bind(&raft_service_t::append, this, _1, _2, _3, _4, _5, _6));
    on<io::raft<msgpack::object>::request_vote>(std::bind(&raft_service_t::request_vote, this, _1, _2, _3, _4));

    this->add("counter", std::make_shared<counter_t>(context, "counter"));
}

const raft::node_id_t&
raft_service_t::id() const {
    return m_self;
}

const std::set<raft::node_id_t>&
raft_service_t::cluster() const {
    return m_cluster;
}

context_t&
raft_service_t::context() {
    return m_context;
}

io::reactor_t&
raft_service_t::reactor() {
    return m_reactor;
}

uint64_t
raft_service_t::election_timeout() const {
    return m_election_timeout;
}

uint64_t
raft_service_t::heartbeat_timeout() const {
    return m_heartbeat_timeout;
}

std::tuple<uint64_t, bool>
raft_service_t::append(const std::string& state_machine,
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

std::tuple<uint64_t, bool>
raft_service_t::request_vote(const std::string& state_machine,
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
