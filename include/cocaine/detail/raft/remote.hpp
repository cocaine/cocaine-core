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

#ifndef COCAINE_RAFT_REMOTE_HPP
#define COCAINE_RAFT_REMOTE_HPP

#include "cocaine/detail/client.hpp"
#include "cocaine/idl/raft.hpp"
#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/literal.hpp"

#include "cocaine/logging.hpp"

#include <algorithm>

namespace cocaine { namespace raft {

// This class holds communication with remote Raft node. It replicates entries and provides method
// to request vote.
template<class Cluster>
class remote_node {
    COCAINE_DECLARE_NONCOPYABLE(remote_node)

    typedef Cluster cluster_type;
    typedef typename cluster_type::actor_type actor_type;
    typedef typename actor_type::entry_type entry_type;
    typedef typename actor_type::snapshot_type snapshot_type;

    // This class handles response from remote node on append request.
    class vote_handler_t {
    public:
        vote_handler_t(remote_node &remote):
            m_active(true),
            m_remote(remote)
        { }

        void
        handle(boost::variant<std::error_code, std::tuple<uint64_t, bool>> result) {
            // If the request is outdated, do nothing.
            if(!m_active) {
                return;
            }

            // Keep our client alive while the handler works.
            auto client = m_remote.m_client;
            (void)client;

            // Parent node stores pointer to handler of current uncompleted request.
            // We should reset this pointer, when the request becomes completed.
            m_remote.reset_vote_state();

            // Do nothing if the request has failed.
            if(boost::get<std::tuple<uint64_t, bool>>(&result)) {
                const auto &response = boost::get<std::tuple<uint64_t, bool>>(result);

                if(std::get<0>(response) > m_remote.m_actor.config().current_term()) {
                    // Stepdown to follower state if we live in old term.
                    m_remote.m_actor.step_down(std::get<0>(response));
                    return;
                } else if(std::get<1>(response)) {
                    m_remote.m_won_term = m_remote.m_actor.config().current_term();
                    m_remote.m_cluster.register_vote();
                }
            }
        }

        // If the remote node doesn't need result of this request, it makes the handler inactive.
        // TODO: rewrite this logic to use upstream_t::revoke method.
        void
        disable() {
            m_active = false;
        }

    private:
        bool m_active;
        remote_node &m_remote;
    };

    // This class handles response from remote node on append request.
    class append_handler_t {
    public:
        append_handler_t(remote_node &remote):
            m_active(true),
            m_remote(remote),
            m_last_index(0)
        { }

        // Set index of last entry replicated with this request.
        void
        set_last(uint64_t last_index) {
            m_last_index = last_index;
        }

        void
        handle(boost::variant<std::error_code, std::tuple<uint64_t, bool>> result) {
            // If the request is outdated, do nothing.
            if(!m_active) {
                return;
            }

            // Keep our client alive while the handler works.
            auto client = m_remote.m_client;
            (void)client;

            // Parent node stores pointer to handler of current uncompleted append request.
            // We should reset this pointer, when the request becomes completed.
            m_remote.reset_append_state();

            // Do nothing if the request has failed.
            // The next append request will be sent with the next heartbeat.
            if(boost::get<std::tuple<uint64_t, bool>>(&result)) {
                const auto &response = boost::get<std::tuple<uint64_t, bool>>(result);

                if(std::get<0>(response) > m_remote.m_actor.config().current_term()) {
                    // Stepdown to follower state if we live in old term.
                    m_remote.m_actor.step_down(std::get<0>(response));
                    return;
                } else if(std::get<1>(response)) {
                    // Mark entries replicated and update commit index, if remote node returned success.
                    m_remote.m_next_index = std::max(m_last_index + 1, m_remote.m_next_index);
                    if(m_remote.m_match_index < m_last_index) {
                        m_remote.m_match_index = m_last_index;
                        m_remote.m_cluster.update_commit_index();
                    }

                    /* COCAINE_LOG_DEBUG(
                        m_remote.m_logger,
                        "Append request has been accepted. "
                        "New match index: %d, next entry to replicate: %d.",
                        m_remote.m_match_index,
                        m_remote.m_next_index
                    ); */
                } else if(m_remote.m_next_index > 1) {
                    // If follower discarded current request, try to replicate older entries.
                    m_remote.m_next_index -= std::min<uint64_t>(
                        m_remote.m_actor.options().message_size,
                        m_remote.m_next_index - 1
                    );

                    /* COCAINE_LOG_DEBUG(
                        m_remote.m_logger,
                        "Append request has been discarded. "
                        "Match index: %d, next entry to replicate: %d.",
                        m_remote.m_match_index,
                        m_remote.m_next_index
                    ); */
                } else {
                    // The remote node discarded our oldest entries.
                    // There is no sense to retry immediately.
                    // Probably there is no sense to retry at all, but what should the node do then?
                    return;
                }

                // Continue replication. If there is no entries to replicate, this call does nothing.
                m_remote.replicate();
            }
        }

        // If the remote node doesn't need result of this request, it makes the handler inactive.
        // TODO: rewrite this logic to use upstream_t::revoke method.
        void
        disable() {
            m_active = false;
        }

    private:
        bool m_active;
        remote_node &m_remote;

        // Last entry replicated with this request.
        uint64_t m_last_index;
    };

public:
    remote_node(cluster_type& cluster, node_id_t id):
        m_cluster(cluster),
        m_actor(cluster.actor()),
        m_logger(new logging::log_t(
            m_actor.context(),
            "raft/" + m_actor.name() + "/remote/" + cocaine::format("%s:%d", id.first, id.second)
        )),
        m_id(id),
        m_heartbeat_timer(m_actor.reactor().native()),
        m_next_index(std::max<uint64_t>(1, m_actor.log().last_index())),
        m_match_index(0),
        m_won_term(0)
    {
        m_heartbeat_timer.set<remote_node, &remote_node::heartbeat>(this);
    }

    ~remote_node() {
        finish_leadership();
    }

    const node_id_t&
    id() const {
        return m_id;
    }

    // Raft actor requests vote from the remote node via this method.
    void
    request_vote() {
        if (m_won_term >= m_actor.config().current_term()) {
            return;
        } else if (m_id == m_actor.config().id()) {
            m_won_term = m_actor.config().current_term();
            m_cluster.register_vote();
        } else if (!m_vote_state) {
            m_vote_state = std::make_shared<vote_handler_t>(*this);
            ensure_connection(std::bind(&remote_node::request_vote_impl, this));
        }
    }

    // When new entries are added to the log,
    // Raft actor tells this class that it's time to replicate.
    void
    replicate() {
        // TODO: Now leader sends one append request at the same time.
        // Probably it's possible to send requests in pipeline manner.
        // I should investigate this question.
        if(m_id == m_actor.config().id()) {
            m_match_index = m_actor.log().last_index();
            m_cluster.update_commit_index();
        } else if(!m_append_state &&
                  m_actor.is_leader() &&
                  m_actor.log().last_index() >= m_next_index)
        {
            // Create new append state to mark, that there is active append request.
            m_append_state = std::make_shared<append_handler_t>(*this);
            ensure_connection(std::bind(&remote_node::replicate_impl, this));
        }
    }

    // Index of last entry replicated to the remote node.
    uint64_t
    match_index() const {
        return m_match_index;
    }

    // Index of last entry replicated to the remote node.
    uint64_t
    won_term() const {
        return m_won_term;
    }

    // Begin leadership. Actually it starts to send heartbeats.
    void
    begin_leadership() {
        if(m_id == m_actor.config().id()) {
            m_match_index = m_actor.log().last_index();
            m_next_index = m_match_index + 1;
        } else {
            m_heartbeat_timer.start(0.0, float(m_actor.options().heartbeat_timeout) / 1000.0);
            // Now we don't know which entries are replicated to the remote.
            m_match_index = 0;
            m_next_index = std::max<uint64_t>(1, m_actor.log().last_index());
        }
    }

    // Stop sending heartbeats.
    void
    finish_leadership() {
        if(m_heartbeat_timer.is_active()) {
            m_heartbeat_timer.stop();
        }
        reset();
    }

    // Reset current state of remote node.
    void
    reset() {
        // Close connection.
        m_resolver.reset();
        m_client.reset();

        // Drop current requests.
        reset_vote_state();
        reset_append_state();

        // If connection error has occurred, then we don't know what entries are replicated.
        m_match_index = 0;
        m_next_index = std::max<uint64_t>(1, m_actor.log().last_index());
    }

private:
    // Drop current append request.
    void
    reset_append_state() {
        if(m_append_state) {
            m_append_state->disable();
            m_append_state.reset();
        }
    }

    // Drop current vote request.
    void
    reset_vote_state() {
        if(m_vote_state) {
            m_vote_state->disable();
            m_vote_state.reset();
        }
    }

    // Election stuff.

    void
    request_vote_impl() {
        if(m_client) {
            COCAINE_LOG_DEBUG(m_logger, "Sending vote request.");

            auto handler = std::bind(&vote_handler_t::handle, m_vote_state, std::placeholders::_1);

            m_client->call<typename io::raft<entry_type, snapshot_type>::request_vote>(
                make_proxy<std::tuple<uint64_t, bool>>(handler, m_actor.context(), m_id.first),
                m_actor.name(),
                m_actor.config().current_term(),
                m_actor.config().id(),
                std::make_tuple(m_actor.log().last_index(), m_actor.log().last_term())
            );
        } else {
            COCAINE_LOG_DEBUG(m_logger, "Client isn't connected. Unable to send vote request.");
            m_vote_state->handle(std::error_code());
        }
    }

    // Leadership stuff.

    void
    replicate_impl() {
        if(!m_client || !m_actor.is_leader()) {
            COCAINE_LOG_DEBUG(m_logger,
                              "Client isn't connected or the local node is not the leader. "
                              "Unable to send append request.");
            m_append_state->handle(std::error_code());
        } else if(m_next_index <= m_actor.log().snapshot_index()) {
            // If leader is far behind the leader, send snapshot.
            send_apply();
        } else if(m_next_index <= m_actor.log().last_index()) {
            // If there are some entries to replicate, then send them to the follower.
            send_append();
        }
    }

    void
    send_apply() {
        auto handler = std::bind(&append_handler_t::handle, m_append_state, std::placeholders::_1);
        auto dispatch = make_proxy<std::tuple<uint64_t, bool>>(handler, m_actor.context());

        auto snapshot_entry = std::make_tuple(
            m_actor.log().snapshot_index(),
            m_actor.log().snapshot_term()
        );

        m_append_state->set_last(m_actor.log().snapshot_index());

        m_client->call<typename io::raft<entry_type, snapshot_type>::apply>(
            dispatch,
            m_actor.name(),
            m_actor.config().current_term(),
            m_actor.config().id(),
            snapshot_entry,
            m_actor.log().snapshot(),
            m_actor.config().commit_index()
        );

        COCAINE_LOG_DEBUG(m_logger,
                          "Sending apply request; term %d; next %d; index %d.",
                          m_actor.config().current_term(),
                          m_next_index,
                          m_actor.log().snapshot_index());
    }

    void
    send_append() {
        auto handler = std::bind(&append_handler_t::handle, m_append_state, std::placeholders::_1);
        auto dispatch = make_proxy<std::tuple<uint64_t, bool>>(handler, m_actor.context());

        // Term of prev_entry. Probably this logic could be in the log,
        // but I wanted to make log implementation as simple as possible,
        // because user can implement own log.
        // TODO: now m_actor.log() is a wrapper around user defined log, so I can move this logic there.
        uint64_t prev_term = (m_actor.log().snapshot_index() + 1 == m_next_index) ?
                             m_actor.log().snapshot_term() :
                             m_actor.log()[m_next_index - 1].term();

        // Index of last entry replicated with this request.
        uint64_t last_index = 0;

        // Send at most options().message_size entries in one message.
        if(m_next_index + m_actor.options().message_size <= m_actor.log().last_index()) {
            last_index = m_next_index + m_actor.options().message_size - 1;
        } else {
            last_index = m_actor.log().last_index();
        }

        // This vector stores entries to be sent.

        // TODO: We can send entries without copying.
        // We just need to implement type_traits to something like boost::Range.
        std::vector<entry_type> entries;

        for(uint64_t i = m_next_index; i <= last_index; ++i) {
            entries.push_back(m_actor.log()[i]);
        }

        m_append_state->set_last(last_index);

        m_client->call<typename io::raft<entry_type, snapshot_type>::append>(
            dispatch,
            m_actor.name(),
            m_actor.config().current_term(),
            m_actor.config().id(),
            std::make_tuple(m_next_index - 1, prev_term),
            entries,
            m_actor.config().commit_index()
        );

        COCAINE_LOG_DEBUG(m_logger,
                          "Sending append request; term %d; next %d; last %d.",
                          m_actor.config().current_term(),
                          m_next_index,
                          m_actor.log().last_index());
    }

    void
    send_heartbeat() {
        if(m_client) {
            COCAINE_LOG_DEBUG(m_logger, "Sending heartbeat.");

            std::tuple<uint64_t, uint64_t> prev_entry(0, 0);

            // Actually we don't need correct prev_entry to heartbeat,
            // but the follower will not accept commit index from request with old prev_entry.
            if(m_next_index - 1 <= m_actor.log().snapshot_index()) {
                prev_entry = std::make_tuple(m_actor.log().snapshot_index(),
                                             m_actor.log().snapshot_term());
            } else if(m_next_index - 1 <= m_actor.log().last_index()) {
                prev_entry = std::make_tuple(m_next_index - 1,
                                             m_actor.log()[m_next_index - 1].term());
            }

            m_client->call<typename io::raft<entry_type, snapshot_type>::append>(
                std::shared_ptr<io::dispatch_t>(),
                m_actor.name(),
                m_actor.config().current_term(),
                m_actor.config().id(),
                prev_entry,
                std::vector<entry_type>(),
                m_actor.config().commit_index()
            );
        }
    }

    void
    heartbeat(ev::timer&, int) {
        if(m_actor.is_leader()) {
            if(m_append_state || m_next_index > m_actor.log().last_index()) {
                // If there is nothing to replicate or there is active append request,
                // just send heartbeat.
                ensure_connection(std::bind(&remote_node::send_heartbeat, this));
            } else {
                replicate();
            }
        }
    }

    // Connection maintenance.

    // Creates connection to remote Raft service and calls continuation (handler).
    void
    ensure_connection(const std::function<void()>& handler) {
        if(m_client) {
            // Connection already exists.
            handler();
        } else {
            COCAINE_LOG_DEBUG(m_logger, "Client is not connected. Connecting...");

            m_resolver = std::make_shared<service_resolver_t>(
                m_actor.context(),
                m_actor.reactor(),
                io::resolver<io::tcp>::query(m_id.first, m_id.second),
                m_actor.context().config.raft.service_name
            );

            using namespace std::placeholders;

            m_resolver->bind(std::bind(&remote_node::on_client_connected, this, handler, _1),
                             std::bind(&remote_node::on_connection_error, this, handler, _1));
        }
    }

    void
    on_client_connected(const std::function<void()>& handler,
                        const std::shared_ptr<client_t>& client)
    {
        m_resolver.reset();

        m_client = client;
        m_client->bind(std::bind(&remote_node::on_error, this, std::placeholders::_1));

        handler();
    }

    void
    on_connection_error(const std::function<void()>& handler,
                        const std::error_code& ec)
    {
        COCAINE_LOG_DEBUG(m_logger,
                          "Unable to connect to Raft service: [%d] %s.",
                          ec.value(),
                          ec.message());
        handler();
        reset();
    }

    void
    on_error(const std::error_code& ec) {
        COCAINE_LOG_DEBUG(m_logger, "Connection error: [%d] %s.", ec.value(), ec.message());
        reset();
    }

private:
    cluster_type &m_cluster;

    actor_type &m_actor;

    const std::unique_ptr<logging::log_t> m_logger;

    // Id of the remote node.
    const node_id_t m_id;

    std::shared_ptr<client_t> m_client;

    std::shared_ptr<service_resolver_t> m_resolver;

    ev::timer m_heartbeat_timer;

    // State of current append request.
    // When there is no active request, this pointer equals nullptr.
    std::shared_ptr<append_handler_t> m_append_state;

    std::shared_ptr<vote_handler_t> m_vote_state;

    // The next log entry to send to the follower.
    uint64_t m_next_index;

    // The last entry replicated to the follower.
    uint64_t m_match_index;

    // The last term, in which the node received vote from the remote.
    uint64_t m_won_term;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_REMOTE_HPP
