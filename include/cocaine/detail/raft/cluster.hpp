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

#ifndef COCAINE_RAFT_CLUSTER_HPP
#define COCAINE_RAFT_CLUSTER_HPP

#include "cocaine/detail/raft/forwards.hpp"
#include "cocaine/detail/raft/remote.hpp"

namespace cocaine { namespace raft {

template<class Actor>
class cluster {
    COCAINE_DECLARE_NONCOPYABLE(cluster)

public:
    typedef Actor actor_type;
    typedef cluster<Actor> cluster_type;
    typedef remote_node<cluster_type> remote_type;
    typedef typename actor_type::config_type::cluster_type snapshot_type;

    cluster(actor_type &actor):
        m_actor(actor),
        m_logger(new logging::log_t(m_actor.context(), "raft/" + actor.name())),
        m_replicator(actor.reactor().native())
    {
        create_clients();
        m_replicator.set<cluster, &cluster::replicate_impl>(this);
    }

    actor_type&
    actor() const {
        return m_actor;
    }

    bool
    transitional() const {
        return actor().config().cluster().transitional();
    }

    bool
    has(const node_id_t& node) const {
        return actor().config().cluster().current.count(node) > 0;
    }

    bool
    in_cluster() const {
        if(transitional()) {
            return actor().config().cluster().next->count(actor().config().id()) > 0;
        } else {
            return actor().config().cluster().current.count(actor().config().id()) > 0;
        }
    }

    void
    insert(const node_id_t& node) {
        COCAINE_LOG_DEBUG(m_logger, "Insert node: %s:%d", node.first, node.second);
        actor().config().cluster().insert(node);
        m_next = m_current;
        m_next.emplace_back(std::make_shared<remote_type>(*this, node));

        // Stepdown to disable self if we are not in cluster now or just become a follower if we were a leader
        // (because leadership in old configuration diesn't mean leadership in new configuration).
        // If this node is a leader now and in the cluster,
        // it starts new election immediately to become a leader in new configuration.
        actor().step_down(actor().config().current_term(), actor().is_leader());
    }

    void
    erase(const node_id_t& node) {
        COCAINE_LOG_DEBUG(m_logger, "Erase node: %s:%d", node.first, node.second);
        actor().config().cluster().erase(node);
        for(auto it = m_current.begin(); it != m_current.end(); ++it) {
            if((*it)->id() != node) {
                m_next.push_back(*it);
            }
        }

        // Stepdown to disable self if we are not in cluster now or just become a follower if we were a leader
        // (because leadership in old configuration diesn't mean leadership in new configuration).
        // If this node is a leader now and in the cluster,
        // it starts new election immediately to become a leader in new configuration.
        actor().step_down(actor().config().current_term(), actor().is_leader());
    }

    void
    commit() {
        COCAINE_LOG_DEBUG(m_logger, "Commit configuration.");
        actor().config().cluster().commit();
        m_current = std::move(m_next);
        m_next = std::vector<std::shared_ptr<remote_type>>();

        // Stepdown to disable self if we are not in cluster.
        if(!in_cluster()) {
            m_actor.step_down(m_actor.config().current_term());
        }
    }

    void
    rollback() {
        COCAINE_LOG_DEBUG(m_logger, "Rollback configuration.");
        actor().config().cluster().rollback();
        m_next = std::vector<std::shared_ptr<remote_type>>();
    }

    void
    consume(const snapshot_type& snapshot) {
        cancel();
        actor().config().cluster() = snapshot;
        create_clients();

        // Stepdown to disable self if we are a leader.
        if(actor().is_leader()) {
            actor().step_down(actor().config().current_term(), true);
        }
    }

    // Handler will be called, when the node wins the election.
    // It's okay that the handler will not be called in case of lost election,
    // because next election anyway will be started on timeout.
    void
    start_election(const std::function<void()>& handler) {
        m_election_handler = handler;

        for(auto it = m_current.begin(); it != m_current.end(); ++it) {
            (*it)->request_vote();
        }

        for(auto it = m_next.begin(); it != m_next.end(); ++it) {
            (*it)->request_vote();
        }
    }

    // Start replication and heartbeating.
    void
    begin_leadership() {
        for(auto it = m_current.begin(); it != m_current.end(); ++it) {
            (*it)->begin_leadership();
        }

        for(auto it = m_next.begin(); it != m_next.end(); ++it) {
            (*it)->begin_leadership();
        }
    }

    // Cancel current operations: election, leadership, appends.
    void
    cancel() {
        for(auto it = m_current.begin(); it != m_current.end(); ++it) {
            (*it)->finish_leadership();
        }

        for(auto it = m_next.begin(); it != m_next.end(); ++it) {
            (*it)->finish_leadership();
        }

        m_election_handler = nullptr;
    }

    // Notify that there is something to replicate.
    void
    replicate() {
        if(!m_replicator.is_active()) {
            m_replicator.start();
        }
    }

    // Calculate commit_index based on information about replicated entries.
    void
    update_commit_index() {
        // Index of last entry replicated to a quorum.
        uint64_t just_committed = get_committed(m_current);

        if(m_next.size() != 0) {
            just_committed = std::min(just_committed, get_committed(m_next));
        }

        // Leader can't assume any new entry to be committed until entry
        // from current term is replicated to a quorum (see commitment restriction in the paper).
        if(just_committed > actor().config().commit_index() &&
            actor().log()[just_committed].term() == actor().config().current_term())
        {
            actor().config().set_commit_index(just_committed);
        }
    }

    // Check if the node has won in the current term.
    void
    register_vote() {
        if (won_elections(m_current) && (m_next.size() == 0 || won_elections(m_next))) {
            if(m_election_handler) {
                m_election_handler();
                m_election_handler = nullptr;
            }
        }
    }

    // Step down if the node is not connected to a quorum.
    void
    check_connections() {
        bool connected_to_quorums = connected_to_quorum(m_current) &&
                                    (m_next.size() == 0 || connected_to_quorum(m_next));

        if(m_actor.is_leader() && !connected_to_quorums) {
            m_actor.step_down(m_actor.config().current_term(), true);
        }
    }

private:
    void
    create_clients() {
        std::vector<node_id_t> intersec;

        if(m_actor.config().cluster().next) {
            std::set_intersection(m_actor.config().cluster().current.begin(),
                                  m_actor.config().cluster().current.end(),
                                  m_actor.config().cluster().next->begin(),
                                  m_actor.config().cluster().next->end(),
                                  std::back_inserter(intersec));
        }

        std::vector<node_id_t> diff1;
        std::set_difference(m_actor.config().cluster().current.begin(),
                            m_actor.config().cluster().current.end(),
                            intersec.begin(),
                            intersec.end(),
                            std::back_inserter(diff1));

        std::vector<node_id_t> diff2;
        if(m_actor.config().cluster().next) {
            std::set_difference(m_actor.config().cluster().next->begin(),
                                m_actor.config().cluster().next->end(),
                                intersec.begin(),
                                intersec.end(),
                                std::back_inserter(diff2));
        }

        std::vector<std::shared_ptr<remote_type>> common_remotes;

        for(auto it = intersec.begin(); it != intersec.end(); ++it) {
            common_remotes.emplace_back(std::make_shared<remote_type>(*this, *it));
            COCAINE_LOG_DEBUG(m_logger, "Add common remote: %s:%d", it->first, it->second);
        }

        m_current = common_remotes;
        for(auto it = diff1.begin(); it != diff1.end(); ++it) {
            m_current.emplace_back(std::make_shared<remote_type>(*this, *it));
            COCAINE_LOG_DEBUG(m_logger, "Add current remote: %s:%d", it->first, it->second);
        }

        m_next = std::move(common_remotes);
        for(auto it = diff2.begin(); it != diff2.end(); ++it) {
            m_next.emplace_back(std::make_shared<remote_type>(*this, *it));
            COCAINE_LOG_DEBUG(m_logger, "Add next remote: %s:%d", it->first, it->second);
        }
    }

    static
    bool
    compare_match_index(const std::shared_ptr<remote_type>& left,
                        const std::shared_ptr<remote_type>& right)
    {
        return left->match_index() < right->match_index();
    }

    // Compute index of last entry committed to set of nodes.
    uint64_t
    get_committed(std::vector<std::shared_ptr<remote_type>> &nodes) {
        if(nodes.size() == 0) {
            return 0;
        }

        // If we sort ascending match_index'es of entire cluster (not m_cluster, which is cluster without local node),
        // then median item (if cluster has odd number of nodes) or greatest item of smaller half of match_index'es
        // (if cluster has even number of nodes) will define last entry replicated to a quorum.
        // Pivot is index of this item.
        size_t pivot = (nodes.size() - 1) / 2;

        std::nth_element(nodes.begin(),
                         nodes.begin() + pivot,
                         nodes.end(),
                         &cluster::compare_match_index);

        return nodes[pivot]->match_index();
    }

    // Check if majority of set of nodes has voted for us in the current term.
    bool
    won_elections(const std::vector<std::shared_ptr<remote_type>> &nodes) {
        size_t votes = 0;

        for(auto it = nodes.begin(); it != nodes.end(); ++it) {
            if((*it)->won_term() == actor().config().current_term()) {
                ++votes;
            }
        }

        return votes > nodes.size() / 2;
    }

    bool
    connected_to_quorum(const std::vector<std::shared_ptr<remote_type>> &nodes) {
        size_t disconnected_number = 0;

        for(auto it = nodes.begin(); it != nodes.end(); ++it) {
            if((*it)->disconnected()) {
                ++disconnected_number;
            }
        }

        return disconnected_number < nodes.size() / 2;
    }

    // 'replicate' method starts this background task.
    // When all call requests in reactor queue are processed, event-loop calls this method,
    // which replicates the new entries to remote nodes.
    // Idle watcher is just a way to push many entries to the log and then replicate them at once.
    void
    replicate_impl(ev::idle&, int) {
        m_replicator.stop();

        for(auto it = m_current.begin(); it != m_current.end(); ++it) {
            (*it)->replicate();
        }

        for(auto it = m_next.begin(); it != m_next.end(); ++it) {
            (*it)->replicate();
        }
    }

private:
    actor_type &m_actor;

    const std::unique_ptr<logging::log_t> m_logger;

    std::vector<std::shared_ptr<remote_type>> m_current;

    std::vector<std::shared_ptr<remote_type>> m_next;

    // This watcher replicates entries in background.
    ev::idle m_replicator;

    std::function<void()> m_election_handler;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_CLUSTER_HPP
