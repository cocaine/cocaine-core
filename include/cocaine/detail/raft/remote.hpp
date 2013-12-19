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

#ifndef COCAINE_RAFT_REMOTE_HPP
#define COCAINE_RAFT_REMOTE_HPP

#include "cocaine/detail/client.hpp"
#include "cocaine/idl/streaming.hpp"
#include "cocaine/asio/resolver.hpp"
#include "cocaine/memory.hpp"

#include <boost/optional.hpp>

#include <functional>
#include <string>

namespace cocaine { namespace raft {

namespace detail {

class result_provider :
    public implements<io::streaming_tag<std::tuple<uint64_t, bool>>>
{
    typedef boost::optional<std::tuple<uint64_t, bool>> result_type;
public:
    result_provider(context_t &context,
                    const std::string& name,
                    const std::function<void(result_type)>& callback):
        implements<io::streaming_tag<std::tuple<uint64_t, bool>>>(context, name),
        m_callback(callback)
    {
        using namespace std::placeholders;

        on<io::streaming<std::tuple<uint64_t, bool>>::chunk>(std::bind(&result_provider::on_write, this, _1, _2));
        on<io::streaming<std::tuple<uint64_t, bool>>::error>(std::bind(&result_provider::on_error, this, _1, _2));
        on<io::streaming<std::tuple<uint64_t, bool>>::choke>(std::bind(&result_provider::on_choke, this));
    }

    ~result_provider() {
        if(m_callback) {
            m_callback(boost::none);
        }
    }

private:
    void
    on_write(uint64_t term, bool success) {
        m_callback(std::make_tuple(term, success));
        m_callback = std::function<void(result_type)>();
    }

    void
    on_error(int, std::string) {
        m_callback(boost::none);
        m_callback = std::function<void(result_type)>();
    }

    void
    on_choke() {
        if(m_callback) {
            m_callback(boost::none);
            m_callback = std::function<void(result_type)>();
        }
    }

private:
    std::function<void(result_type)> m_callback;
};

} // namespace detail

template<class Actor>
class remote_node {
    typedef Actor actor_type;

    typedef typename actor_type::log_type log_type;

    class append_state_t {
    public:
        append_state_t(remote_node &remote, uint64_t last_index):
            m_active(true),
            m_remote(remote),
            m_last(last_index)
        {
            // Empty.
        }

        void
        disable() {
            m_active = false;
        }

        void
        handle(boost::optional<std::tuple<uint64_t, bool>> result) {
            if(!m_active) {
                return;
            }

            disable();
            m_remote.reset_append_state();

            if(result) {
                if(std::get<0>(*result) > m_remote.local().current_term()) {
                    m_remote.local().step_down(std::get<0>(*result));
                    return;
                } else if(std::get<1>(*result)) {
                    m_remote.m_next_index = std::max(m_last + 1, m_remote.m_next_index);
                    if(m_remote.m_match_index < m_last) {
                        m_remote.m_match_index = m_last;
                        m_remote.local().update_commit_index();
                    }
                } else {
                    m_remote.m_next_index--;
                }
                m_remote.replicate();
            }
        }

    private:
        bool m_active;
        remote_node &m_remote;
        uint64_t m_last;
    };

public:
    remote_node(actor_type& local, node_id_t id):
        m_id(id),
        m_local(local),
        m_heartbeat_timer(local.service().reactor().native()),
        m_next_index(local.log().last_index()),
        m_match_index(0)
    {
        m_heartbeat_timer.set<remote_node, &remote_node::heartbeat>(this);
    }

    void
    request_vote(const std::function<void(boost::optional<std::tuple<uint64_t, bool>>)>& handler) {
        ensure_connection();

        m_client->call<typename io::raft<typename log_type::value_type>::request_vote>(
            std::make_shared<detail::result_provider>(
                m_local.service().context(),
                m_id.first,
                handler
            ),
            m_local.name(),
            m_local.current_term(),
            m_local.id(),
            std::make_tuple(m_local.log().last_index(), m_local.log().last_term())
        );
    }

    uint64_t
    match_index() const {
        return m_match_index;
    }

    void
    begin_leadership() {
        m_heartbeat_timer.start(0.0, float(m_local.service().heartbeat_timeout()) / 1000.0);
    }

    void
    finish_leadership() {
        if(m_heartbeat_timer.is_active()) {
            m_heartbeat_timer.stop();
        }
        reset();
    }

    void
    replicate() {
        if(m_local.log().last_index() >= m_next_index) {
            send_append();
        }
    }

    void
    reset_append_state() {
        if(m_append_state) {
            m_append_state->disable();
            m_append_state.reset();
        }
    }

    void
    reset() {
        reset_append_state();
        m_client.reset();
        m_next_index = 0;
        m_match_index = 0;
    }

    actor_type&
    local() {
        return m_local;
    }

private:
    void
    send_append() {
        if(m_local.is_leader() && m_client) {
            if(m_append_state) {
                m_client->call<typename io::raft<typename log_type::value_type>::append>(
                    std::shared_ptr<io::dispatch_t>(),
                    m_local.name(),
                    m_local.current_term(),
                    m_local.id(),
                    std::make_tuple(0, 0),
                    std::vector<typename log_type::value_type>(),
                    m_local.commit_index()
                );
            } else {
                m_append_state = std::make_shared<append_state_t>(*this, m_local.log().last_index());
                auto handler = std::bind(&append_state_t::handle, m_append_state, std::placeholders::_1);
                auto dispatch = std::make_shared<detail::result_provider>(
                    m_local.service().context(),
                    m_local.id().first,
                    handler
                );

                m_client->call<typename io::raft<typename log_type::value_type>::append>(
                    dispatch,
                    m_local.name(),
                    m_local.current_term(),
                    m_local.id(),
                    std::make_tuple(m_next_index - 1, m_local.log().at(m_next_index - 1).term()),
                    std::vector<typename log_type::value_type>(m_local.log().iter(m_next_index), m_local.log().end()),
                    m_local.commit_index()
                );
            }
        }
    }

    void
    heartbeat(ev::timer&, int) {
        ensure_connection();
        send_append();
    }

    void
    ensure_connection() {
        if(m_client) {
            return;
        }

        auto endpoints = io::resolver<io::tcp>::query(m_id.first, m_id.second);

        std::exception_ptr e;

        for(auto it = endpoints.begin(); it != endpoints.end(); ++it) {
            try {
                auto socket = std::make_shared<io::socket<io::tcp>>(*it);
                auto channel = std::make_unique<io::channel<io::socket<io::tcp>>>(
                    m_local.service().reactor(),
                    socket
                );
                m_client = std::make_shared<client_t>(channel);

                return;
            } catch(const std::exception&) {
                e = std::current_exception();
            }
        }

        std::rethrow_exception(e);
    }

    void
    on_error(const std::error_code& ec) {
        reset();
    }

private:
    node_id_t m_id;

    actor_type &m_local;

    std::shared_ptr<client_t> m_client;

    ev::timer m_heartbeat_timer;

    std::shared_ptr<append_state_t> m_append_state;

    // The next log entry to send to the follower.
    uint64_t m_next_index;

    // The last entry replicated to the follower.
    uint64_t m_match_index;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_REMOTE_HPP
