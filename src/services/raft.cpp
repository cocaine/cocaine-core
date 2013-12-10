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

using namespace cocaine::io;
using namespace cocaine::service;

using namespace std::placeholders;

const unsigned int election_timeout = 500;
const unsigned int heartbeat_timeout = election_timeout / 2;

raft_actor_t::raft_actor_t(context_t& context,
                           io::reactor_t& reactor,
                           const std::string& name,
                           const dynamic_t& args) :
    api::service_t(context, reactor, name, args),
    implements<io::raft_tag>(context, name),
    m_context(context),
    m_reactor(reactor),
    m_election_timer(reactor),
    m_applier(reactor)
{
    using namespace std::placeholders;

    on<io::raft::append>(std::bind(&raft_actor_t::on_append, this, _1, _2, _3, _4, _5));
    on<io::raft::vote>(std::bind(&raft_actor_t::on_vote, this, _1, _2, _3));

    m_election_timer.set<raft_actor_t, &raft_actor_t::on_disown>(this);
    restart_election_timer();

    m_heartbeat_timer.set<raft_actor_t, &raft_actor_t::send_heartbeats>(this);

    m_applier.set<raft_actor_t, &raft_actor_t::on_apply_entries>(this);
}

std::tuple<uint64_t, bool>
raft_actor_t::on_append(uint64_t term,
                        node_id_t leader,
                        std:tuple<uint64_t, uint64_t> prev_entry, // index, term
                        const std::vector<log_entry_t>& entries,
                        uint64_t commit_index)
{
    if(term < m_current_term) {
        return std::make_tuple(m_current_term, false);
    }

    step_down(term);

    m_leader = leader;

    if(std::get<0>(prev_entry) > m_log.size()) {
        return std::make_tuple(m_current_term, false);
    }

    if(std::get<1>(prev_entry) != m_log[std::get<0>(prev_entry)].term) {
        return std::make_tuple(m_current_term, false);
    }

    uint64_t entry_index = std::get<0>(prev_entry);
    for(auto it = entries.begin(); it != entries.end(); ++it, ++entry_index) {
        if(m_log.size() > entry_index && it->term != m_log[entry_index].term) {
            m_log.resize(entry_index);
        }

        if(entry_index >= m_log.size()) {
            append(*it);
        }
    }

    m_commit_index = commit_index;

    if(m_commit_index > m_last_applied && !m_applier.started()) {
        m_applier.start();
    }

    return std::make_tuple(m_current_term, true);
}

std::tuple<uint64_t, bool>
raft_actor_t::on_vote(uint64_t term,
                      node_id_t candidate,
                      std:tuple<uint64_t, uint64_t> last_entry /* index, term */)
{
    if(term > m_current_term) {
        step_down(term);
    }

    if(term == m_current_term &&
       !m_voted_for &&
       (std::get<1>(last_entry) > m_log.back().term ||
        std::get<1>(last_entry) == m_log.back().term &&
        std::get<0>(last_entry) >= m_log.size()))
    {
        step_down(term);
        m_voted_for = candidate;
    }

    return std::make_tuple(m_current_term, term == m_current_term && m_voted_for == candidate);
}

void
raft_actor_t::step_down(uint64_t term) {
    if(term > m_current_term) {
        m_current_term = term;
        voted_for.reset();
    }

    restart_election_timer();

    // TODO: Disable all non-follower activity.
    reset_election_state();
}

void
raft_actor_t::on_disown(ev::timer&, int) {
    start_election();
}

void
raft_actor_t::on_apply_entries(ev::idle&, int) {
    if(m_commit_index == m_last_applied) {
        m_applier.stop();
        return;
    }

    for(size_t entry = m_last_applied + 1; entry <= m_commit_index; ++entry) {
        if(m_log[entry].type == log_entry_t::command) {
            m_state_machine(m_log[entry].data);
        }

        ++m_last_applied;
    }
}

void
raft_actor_t::append(const log_entry_t& entry) {
    m_log.push_back(entry);
}

void
raft_actor_t::stop_election_timer() {
    if(!m_election_timer.started()) {
        m_election_timer.stop();
    }
}

void
raft_actor_t::restart_election_timer() {
    stop_election_timer();
    m_election_timer.start(float(election_timeout + rand() % election_timeout) / 1000.0);
}

class raft_actor_t::election_state_t {
public:
    election_state_t(raft_actor_t &actor):
        m_active(true),
        m_votes(0),
        m_actor(actor)
    {
        // Empty.
    }

    void
    disable() {
        m_active = false;
    }

    void
    vote_handler(boost::optional<std::tuple<uint64_t, bool>> result) {
        if(!m_active) {
            return;
        }

        if(result) {
            if(std::get<1>(*result)) {
                m_granted++;

                if(m_granted > m_actor.cluster_size() / 2) {
                    m_active = false;
                    m_actor.switch_to_leader();
                }
            } else if(std::get<0>(*result) > m_actor.term()) {
                m_active = false;
                m_actor.step_down(std::get<0>(*result));
            }
        }
    }

private:
    bool m_active;
    unsigned int m_granted;
    raft_actor_t &m_actor;
};

void
raft_actor_t::start_election() {
    using namespace std::placeholders;

    restart_election_timer();
    reset_election_state();

    m_election_state = std::make_shared<election_state_t>(*this);

    for(auto it = m_cluster.begin(); it != m_cluster.end(); ++it) {
        try {
            it->second.request_vote(std::bind(&election_state_t::vote_handler, m_election_state, _1),
                                    m_current_term,
                                    m_id,
                                    std::make_tuple(m_log.size(), m_log.back().term));
        } catch(const std::exception&) {
            // Ignore.
        }
    }
}

void
raft_actor_t::reset_election_state() {
    if(m_election_state) {
        m_election_state->disable();
        m_election_state.reset();
    }
}

void
raft_actor_t::switch_to_leader() {
    stop_election_timer();
    reset_election_state();

    m_heartbeat_timer.start(0.0f, float(heartbeat_timeout) / 1000.0);
}

remote_node_t::remote_node_t(raft_actor_t& local, node_id_t id):
    m_id(id),
    m_local(local),
    m_next_index(0),
    m_match_index(0)
{
    // Empty.
}

namespace {

template<class Result>
class result_provider :
    public implements<io::streaming_tag<Result>>
{
    result_provider(context_t &context,
                    const std::string& name,
                    const std::function<void(boost::optional<Result>)>& callback):
        implements<io::streaming_tag<Result>>(context, name),
        m_callback(callback)
    {
        using namespace std::plaseholders;

        on<io::streaming<Result>::chunk>(std::bind(&result_provider::on_write, this, _1));
        on<io::streaming<Result>::error>(std::bind(&result_provider::on_error, this, _1, _2));
        on<io::streaming<Result>::choke>(std::bind(&result_provider::on_choke, this));
    }

private:
    void
    on_write(const Result& r) {
        m_callback(r);
    }

    void
    on_error(int, std::string) {
        m_callback(boost::optional<Result>());
    }

    void
    on_choke() const {
        // Empty.
    }

private:
    std::function<void(boost::optional<Result>)> m_callback;
};

} // namespace

void
remote_node_t::request_vote(
    const std::function<void(boost::optional<std::tuple<uint64_t, bool>>)>& handler,
    uint64_t term,
    node_id_t candidate,
    std:tuple<uint64_t, uint64_t> last_entry
) {
    ensure_connection();

    m_client->call<io::raft::vote>(
        std::make_shared<result_provider<std::tuple<uint64_t, bool>>>(m_context, m_id.first, handler),
        term,
        candidate,
        last_entry
    );
}

void
remote_node_t::replicate(
    const std::function<void(boost::optional<std::tuple<uint64_t, bool>>)>& handler,
    uint64_t term,
    node_id_t candidate,
    std:tuple<uint64_t, uint64_t> last_entry
    const std::vector<log_entry_t>& entries,
    uint64_t commit_index
) {
    ensure_connection();

    m_client->call<io::raft::append>(
        std::make_shared<result_provider<std::tuple<uint64_t, bool>>>(m_context, m_id.first, handler),
        term,
        candidate,
        last_entry,
        entries,
        commit_index
    );
}

void
remote_node_t::on_error(const std::error_code& ec) {
    reset();
}

void
remote_node_t::reset() {
    m_client.reset();
    m_next_index = 0;
    m_match_index = 0;
}

void
remote_node_t::ensure_connection() {
    if(m_client) {
        return;
    }

    auto endpoints = io::resolver<io::tcp>::query(m_id.first, m_id.second);

    std::exception_ptr e;

    for(auto it = endpoints.begin(); it != endpoints.end(); ++it) {
        try {
            auto socket = std::make_shared<io::socket<io::tcp>>(*it);
            m_client = std::make_shared<client_t>(
                io::channel<io::socket<io::tcp>>(m_local.reactor(), socket)
            );

            return;
        } catch(const std::exception&) {
            e = std::current_exception();
        }
    }

    std::rethrow_exception(e);
}
