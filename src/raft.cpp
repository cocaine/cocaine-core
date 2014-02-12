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

#include "cocaine/detail/raft/service.hpp"
#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include <iostream>

using namespace cocaine;
using namespace cocaine::service;

std::error_code
cocaine::make_error_code(raft_errc e) {
    return std::error_code(static_cast<int>(e), raft_category());
}

std::error_condition
cocaine::make_error_condition(raft_errc e) {
    return std::error_condition(static_cast<int>(e), raft_category());
}

const std::shared_ptr<raft::actor_concept_t>&
raft::repository_t::get(const std::string& name) const {
    auto actors = m_actors.synchronize();

    auto it = actors->find(name);

    if(it != actors->end()) {
        return it->second;
    } else {
        throw error_t("There is no such state machine.");
    }
}

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
        typedef int result_type;
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
        typedef int result_type;
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
        typedef bool result_type;
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

struct counter_t {
    typedef io::counter_tag tag;
    typedef int snapshot_type;

    counter_t(context_t& context, const std::string& name):
        m_log(new logging::log_t(context, name)),
        m_value(0)
    {
        COCAINE_LOG_DEBUG(m_log, "Initialize counter state machine %s.", name);
    }

    counter_t(counter_t&& other) {
        *this = std::move(other);
    }

    counter_t&
    operator=(counter_t&& other) {
        m_log = std::move(other.m_log);
        m_value = other.m_value.load();

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

    int
    operator()(const io::aux::frozen<io::counter::inc>& req) {
        auto val = m_value.fetch_add(std::get<0>(req.tuple)) + std::get<0>(req.tuple);
        COCAINE_LOG_DEBUG(m_log, "Add value to counter: %d. Value is %d.", std::get<0>(req.tuple), val);

        return val;
    }

    int
    operator()(const io::aux::frozen<io::counter::dec>& req) {
        auto val = m_value.fetch_sub(std::get<0>(req.tuple)) - std::get<0>(req.tuple);
        COCAINE_LOG_DEBUG(m_log, "Subtract value from counter: %d. Value is %d.", std::get<0>(req.tuple), val);

        return val;
    }

    bool
    operator()(const io::aux::frozen<io::counter::cas>& req) {
        int expected, desired;
        std::tie(expected, desired) = req.tuple;
        auto res = m_value.compare_exchange_strong(expected, desired);
        COCAINE_LOG_DEBUG(m_log, "Compare and swap: %d, %d: %s.", expected, desired, res ? "sucess" : "fail");

        return res;
    }

private:
    std::unique_ptr<logging::log_t> m_log;
    std::atomic<int> m_value;
};

raft_t::raft_t(context_t& context,
               io::reactor_t& reactor,
               const std::string& name):
    api::service_t(context, reactor, name, dynamic_t::empty_object),
    implements<io::raft_tag<msgpack::object, msgpack::object>>(context, name),
    m_context(context),
    m_reactor(reactor),
    m_log(new logging::log_t(context, name)),
    m_test_timer(reactor.native())
{
    COCAINE_LOG_DEBUG(m_log, "Starting RAFT service with name %s.", name);

    using namespace std::placeholders;

    on<io::raft<msgpack::object, msgpack::object>::append>(std::bind(&raft_t::append, this, _1, _2, _3, _4, _5, _6));
    on<io::raft<msgpack::object, msgpack::object>::apply>(std::bind(&raft_t::apply, this, _1, _2, _3, _4, _5, _6));
    on<io::raft<msgpack::object, msgpack::object>::request_vote>(std::bind(&raft_t::request_vote, this, _1, _2, _3, _4));

    // TODO: Remove this code after testing.
    COCAINE_LOG_DEBUG(m_log, "Adding 'counter' state machine to the RAFT.");

    context.raft.insert("counter", counter_t(context, "counter"));

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
    return m_context.raft.get(state_machine)->append(term,
                                                     leader,
                                                     prev_entry,
                                                     entries,
                                                     commit_index);
}

deferred<std::tuple<uint64_t, bool>>
raft_t::apply(const std::string& state_machine,
              uint64_t term,
              raft::node_id_t leader,
              std::tuple<uint64_t, uint64_t> snapshot_entry, // index, term
              const msgpack::object& snapshot,
              uint64_t commit_index)
{
    return m_context.raft.get(state_machine)->apply(term,
                                                    leader,
                                                    snapshot_entry,
                                                    snapshot,
                                                    commit_index);
}

deferred<std::tuple<uint64_t, bool>>
raft_t::request_vote(const std::string& state_machine,
                     uint64_t term,
                     raft::node_id_t candidate,
                     std::tuple<uint64_t, uint64_t> last_entry)
{
    return m_context.raft.get(state_machine)->request_vote(term, candidate, last_entry);
}

namespace {
    void
    on_inc_result(const boost::variant<int, std::error_code>& res) {
        const std::error_code *ec = boost::get<std::error_code>(&res);

        if(ec) {
            std::cerr << "Inc error: " << ec->value() << ", " << ec->message() << std::endl;
        } else {
            std::cerr << "Inc success: " << boost::get<int>(res) << std::endl;
        }
    }

    void
    on_dec_result(const boost::variant<int, std::error_code>& res) {
        const std::error_code *ec = boost::get<std::error_code>(&res);

        if(ec) {
            std::cerr << "Dec error: " << ec->value() << ", " << ec->message() << std::endl;
        } else {
            std::cerr << "Dec success: " << boost::get<int>(res) << std::endl;
        }
    }
}

void
raft_t::do_something(ev::timer&, int) {
    auto c = m_context.raft.get("counter");

    typedef raft::actor<counter_t, raft::configuration<raft::log<counter_t>>> actor_type;

    actor_type *a = static_cast<actor_type*>(c.get());

    if(rand() % 2 == 0) {
        a->call<io::counter::dec>(&on_inc_result, rand() % 50);
    } else {
        a->call<io::counter::inc>(&on_dec_result, rand() % 50);
    }
}
