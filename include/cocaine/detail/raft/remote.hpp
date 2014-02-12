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

#include <boost/optional.hpp>
#include <algorithm>

namespace cocaine { namespace raft {

namespace detail {

class raft_dispatch :
    public implements<io::streaming_tag<std::tuple<uint64_t, bool>>>
{
    typedef boost::optional<std::tuple<uint64_t, bool>> result_type;
public:
    raft_dispatch(context_t &context,
                  const std::string& name,
                  const std::function<void(result_type)>& callback):
        implements<io::streaming_tag<std::tuple<uint64_t, bool>>>(context, name),
        m_callback(callback)
    {
        using namespace std::placeholders;

        on<io::streaming<std::tuple<uint64_t, bool>>::chunk>(std::bind(&raft_dispatch::on_write, this, _1, _2));
        on<io::streaming<std::tuple<uint64_t, bool>>::error>(std::bind(&raft_dispatch::on_error, this, _1, _2));
        on<io::streaming<std::tuple<uint64_t, bool>>::choke>(std::bind(&raft_dispatch::on_choke, this));
    }

    ~raft_dispatch() {
        if(m_callback) {
            m_callback(boost::none);
            m_callback = nullptr;
        }
    }

private:
    void
    on_write(uint64_t term, bool success) {
        if(m_callback) {
            m_callback(std::make_tuple(term, success));
            m_callback = nullptr;
        }
    }

    void
    on_error(int, const std::string&) {
        if(m_callback) {
            m_callback(boost::none);
            m_callback = nullptr;
        }
    }

    void
    on_choke() {
        if(m_callback) {
            m_callback(boost::none);
            m_callback = nullptr;
        }
    }

private:
    std::function<void(result_type)> m_callback;
};

} // namespace detail

template<class Actor>
class remote_node {
    COCAINE_DECLARE_NONCOPYABLE(remote_node)

    typedef Actor actor_type;

    typedef typename actor_type::log_type log_type;
    typedef typename actor_type::entry_type entry_type;
    typedef typename actor_type::snapshot_type snapshot_type;

    class append_handler_t {
    public:
        append_handler_t(remote_node &remote):
            m_active(true),
            m_remote(remote),
            m_last_index(0)
        { }

        void
        set_last(uint64_t last_index) {
            m_last_index = last_index;
        }

        void
        handle(boost::optional<std::tuple<uint64_t, bool>> result) {
            if(!m_active) {
                return;
            }

            disable();
            m_remote.reset_append_state();

            if(result) {
                if(std::get<0>(*result) > m_remote.m_local.config().current_term()) {
                    m_remote.m_local.step_down(std::get<0>(*result));
                    return;
                } else if(std::get<1>(*result)) {
                    m_remote.m_next_index = std::max(m_last_index + 1, m_remote.m_next_index);
                    if(m_remote.m_match_index < m_last_index) {
                        m_remote.m_match_index = m_last_index;
                        m_remote.m_local.update_commit_index();
                    }
                } else if(m_remote.m_next_index > 1) {
                    m_remote.m_next_index -= std::min<uint64_t>(
                        m_remote.m_local.options().message_size,
                        m_remote.m_next_index - 1
                    );
                }
                m_remote.replicate();
            }
        }

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
    remote_node(actor_type& local, node_id_t id):
        m_logger(new logging::log_t(local.context(), "remote/" + cocaine::format("%s:%d", id.first, id.second))),
        m_id(id),
        m_local(local),
        m_heartbeat_timer(local.reactor().native()),
        m_next_index(std::max<uint64_t>(1, local.config().log().last_index())),
        m_match_index(0)
    {
        m_heartbeat_timer.set<remote_node, &remote_node::heartbeat>(this);
    }

    void
    request_vote(const std::function<void(boost::optional<std::tuple<uint64_t, bool>>)>& handler) {
        ensure_connection(std::bind(&remote_node::request_vote_impl, this, handler));
    }

    void
    replicate() {
        if(!m_append_state &&
           m_local.is_leader() &&
           m_local.config().log().last_index() >= m_next_index)
        {
            m_append_state = std::make_shared<append_handler_t>(*this);
            ensure_connection(std::bind(&remote_node::replicate_impl, this));
        }
    }

    uint64_t
    match_index() const {
        return m_match_index;
    }

    void
    begin_leadership() {
        m_heartbeat_timer.start(0.0, float(m_local.options().heartbeat_timeout) / 1000.0);
        m_match_index = 0;
        m_next_index = std::max<uint64_t>(1, m_local.config().log().last_index());
    }

    void
    finish_leadership() {
        if(m_heartbeat_timer.is_active()) {
            m_heartbeat_timer.stop();
        }
        reset();
    }

    void
    reset() {
        reset_append_state();
        m_resolver.reset();
        m_client.reset();
        m_match_index = 0;
    }

private:
    void
    reset_append_state() {
        if(m_append_state) {
            m_append_state->disable();
            m_append_state.reset();
        }
    }

    // Election stuff.

    void
    request_vote_impl(const std::function<void(boost::optional<std::tuple<uint64_t, bool>>)>& handler) {
        if(m_client) {
            COCAINE_LOG_DEBUG(m_logger, "Sending vote request.");
            m_client->call<typename io::raft<entry_type, snapshot_type>::request_vote>(
                std::make_shared<detail::raft_dispatch>(m_local.context(), m_id.first, handler),
                m_local.name(),
                m_local.config().current_term(),
                m_local.config().id(),
                std::make_tuple(m_local.config().log().last_index(), m_local.config().log().last_term())
            );
        } else {
            COCAINE_LOG_DEBUG(m_logger, "Client isn't connected. Unable to send vote request.");
            handler(boost::none);
        }
    }

    // Leadership stuff.

    void
    replicate_impl() {
        if(!m_client || !m_local.is_leader()) {
            COCAINE_LOG_DEBUG(m_logger, "Client isn't connected or the local node is not the leader. Unable to send append request.");
            m_append_state->handle(boost::none);
        } else if(m_next_index <= m_local.config().log().snapshot_index()) {
            send_apply();
        } else if(m_next_index <= m_local.config().log().last_index()) {
            send_append();
        }
    }

    void
    send_apply() {
        auto handler = std::bind(&append_handler_t::handle, m_append_state, std::placeholders::_1);
        auto dispatch = std::make_shared<detail::raft_dispatch>(m_local.context(), "apply", handler);

        auto snapshot_entry = std::make_tuple(
            m_local.config().log().snapshot_index(),
            m_local.config().log().snapshot_term()
        );

        m_append_state->set_last(m_local.config().log().snapshot_index());

        m_client->call<typename io::raft<entry_type, snapshot_type>::apply>(
            dispatch,
            m_local.name(),
            m_local.config().current_term(),
            m_local.config().id(),
            snapshot_entry,
            m_local.config().log().snapshot(),
            m_local.config().commit_index()
        );

        COCAINE_LOG_DEBUG(m_logger,
                          "Sending apply request; term %d; next %d; index %d.",
                          m_local.config().current_term(),
                          m_next_index,
                          m_local.config().log().snapshot_index());
    }

    void
    send_append() {
        auto handler = std::bind(&append_handler_t::handle, m_append_state, std::placeholders::_1);
        auto dispatch = std::make_shared<detail::raft_dispatch>(m_local.context(), "append", handler);

        uint64_t prev_term = (m_local.config().log().snapshot_index() + 1 == m_next_index) ?
                             m_local.config().log().snapshot_term() :
                             m_local.config().log()[m_next_index - 1].term();

        uint64_t last_index = 0;

        if(m_next_index + m_local.options().message_size <= m_local.config().log().last_index()) {
            last_index = m_next_index + m_local.options().message_size - 1;
        } else {
            last_index = m_local.config().log().last_index();
        }

        std::vector<entry_type> entries;

        for(uint64_t i = m_next_index; i <= last_index; ++i) {
            entries.push_back(m_local.config().log()[i]);
        }

        m_append_state->set_last(last_index);

        m_client->call<typename io::raft<entry_type, snapshot_type>::append>(
            dispatch,
            m_local.name(),
            m_local.config().current_term(),
            m_local.config().id(),
            std::make_tuple(m_next_index - 1, prev_term),
            entries,
            m_local.config().commit_index()
        );

        COCAINE_LOG_DEBUG(m_logger,
                          "Sending append request; term %d; next %d; last %d.",
                          m_local.config().current_term(),
                          m_next_index,
                          m_local.config().log().last_index());
    }

    void
    send_heartbeat() {
        if(m_client) {
            COCAINE_LOG_DEBUG(m_logger, "Sending heartbeat.");

            std::tuple<uint64_t, uint64_t> prev_entry(0, 0);

            if(m_next_index - 1 <= m_local.config().log().snapshot_index()) {
                prev_entry = std::make_tuple(m_local.config().log().snapshot_index(),
                                             m_local.config().log().snapshot_term());
            } else if(m_next_index - 1 <= m_local.config().log().last_index()) {
                prev_entry = std::make_tuple(m_next_index - 1,
                                             m_local.config().log()[m_next_index - 1].term());
            }

            m_client->call<typename io::raft<entry_type, snapshot_type>::append>(
                std::shared_ptr<io::dispatch_t>(),
                m_local.name(),
                m_local.config().current_term(),
                m_local.config().id(),
                prev_entry,
                std::vector<entry_type>(),
                m_local.config().commit_index()
            );
        }
    }

    void
    heartbeat(ev::timer&, int) {
        if(m_local.is_leader()) {
            if(m_append_state || m_next_index > m_local.config().log().last_index()) {
                ensure_connection(std::bind(&remote_node::send_heartbeat, this));
            } else {
                replicate();
            }
        }
    }

    // Connection maintenance.

    void
    ensure_connection(const std::function<void()>& handler) {
        if(m_client) {
            handler();
        } else {
            COCAINE_LOG_DEBUG(m_logger, "Client is not connected. Connecting...");

            m_resolver = std::make_shared<service_resolver_t>(
                m_local.context(),
                m_local.reactor(),
                io::resolver<io::tcp>::query(m_id.first, m_id.second),
                m_local.context().config.raft.service_name
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
        m_resolver.reset();

        COCAINE_LOG_DEBUG(m_logger,
                          "Unable to connect to Raft service: [%d] %s.",
                          ec.value(),
                          ec.message());
        handler();
    }

    void
    on_error(const std::error_code& ec) {
        COCAINE_LOG_DEBUG(m_logger, "Connection error: [%d] %s.", ec.value(), ec.message());
        reset();
    }

private:
    const std::unique_ptr<logging::log_t> m_logger;

    const node_id_t m_id;

    actor_type &m_local;

    std::shared_ptr<client_t> m_client;

    ev::timer m_heartbeat_timer;

    std::shared_ptr<service_resolver_t> m_resolver;

    std::shared_ptr<append_handler_t> m_append_state;

    // The next log entry to send to the follower.
    uint64_t m_next_index;

    // The last entry replicated to the follower.
    uint64_t m_match_index;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_REMOTE_HPP
