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

#include "cocaine/idl/streaming.hpp"
#include "cocaine/idl/locator.hpp"

#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/literal.hpp"

#include "cocaine/asio/resolver.hpp"
#include "cocaine/memory.hpp"
#include "cocaine/logging.hpp"

#include <boost/optional.hpp>

#include <functional>
#include <string>
#include <algorithm>

namespace cocaine { namespace raft {

namespace detail {

class resolve_dispatch :
    public implements<io::locator::resolve::drain_type>
{
    typedef boost::optional<std::tuple<std::string, uint16_t>> result_type;

public:
    resolve_dispatch(context_t &context,
                     const std::string& name,
                     const std::function<void(result_type)>& callback):
        implements<io::locator::resolve::drain_type>(context, name),
        m_callback(callback)
    {
        using namespace std::placeholders;

        typedef io::streaming<io::locator::resolve::value_type> stream_type;

        on<stream_type::chunk>(std::bind(&resolve_dispatch::on_write, this, _1, _2, _3));
        on<stream_type::error>(std::bind(&resolve_dispatch::on_error, this, _1, _2));
        on<stream_type::choke>(std::bind(&resolve_dispatch::on_choke, this));
    }

    ~resolve_dispatch() {
        if(m_callback) {
            m_callback(boost::none);
        }
    }

private:
    void
    on_write(const io::locator::resolve::endpoint_tuple_type& endpoint,
             unsigned int,
             const io::dispatch_graph_t&)
    {
        m_callback(endpoint);
        m_callback = std::function<void(result_type)>();
    }

    void
    on_error(int, const std::string&) {
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
        }
    }

private:
    void
    on_write(uint64_t term, bool success) {
        m_callback(std::make_tuple(term, success));
        m_callback = std::function<void(result_type)>();
    }

    void
    on_error(int, const std::string&) {
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

    class resolve_handler_t {
    public:
        resolve_handler_t(const std::shared_ptr<client_t>& locator,
                          remote_node &remote,
                          std::function<void()> callback):
            m_locator(locator),
            m_active(true),
            m_remote(remote),
            m_callback(callback)
        {
            // Emtpy.
        }

        void
        handle(boost::optional<std::tuple<std::string, uint16_t>> endpoint) {
            if(m_active) {
                if(endpoint) {
                    m_remote.connect(std::get<0>(endpoint.get()), std::get<1>(endpoint.get()));
                }
                m_callback();
            }
        }

        void
        disable() {
            m_active = false;
            m_locator.reset();
        }

    private:
        std::shared_ptr<client_t> m_locator;
        bool m_active;
        remote_node &m_remote;
        std::function<void()> m_callback;
    };

    friend class resolve_handler_t;

    class append_handler_t {
    public:
        append_handler_t(remote_node &remote, uint64_t last_index):
            m_active(true),
            m_remote(remote),
            m_last(last_index)
        {
            // Empty.
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
                } else if(m_remote.m_next_index > 1) {
                    --m_remote.m_next_index;
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
        uint64_t m_last;
    };

public:
    remote_node(actor_type& local, node_id_t id):
        m_logger(new logging::log_t(local.service().context(), "remote/" + cocaine::format("%s:%d", id.first, id.second))),
        m_id(id),
        m_local(local),
        m_heartbeat_timer(local.service().reactor().native()),
        m_next_index(std::max<int64_t>(1, local.log().last_index())),
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
        if(!m_append_state && m_local.is_leader() && m_local.log().last_index() >= m_next_index) {
            m_append_state = std::make_shared<append_handler_t>(*this, m_local.log().last_index());
            ensure_connection(std::bind(&remote_node::send_append, this));
        } else {
            COCAINE_LOG_DEBUG(m_logger, "Nothing to replicate.");
        }
    }

    uint64_t
    match_index() const {
        return m_match_index;
    }

    void
    begin_leadership() {
        m_heartbeat_timer.start(0.0, float(m_local.service().heartbeat_timeout()) / 1000.0);
        m_match_index = 0;
        m_next_index = std::max<int64_t>(1, m_local.log().last_index());
    }

    void
    finish_leadership() {
        if(m_heartbeat_timer.is_active()) {
            m_heartbeat_timer.stop();
        }
        reset();
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
        //COCAINE_LOG_DEBUG(m_logger, "Reset the remote.");
        reset_append_state();
        if(m_resolve_state) {
            m_resolve_state->disable();
            m_resolve_state.reset();
        }
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
    request_vote_impl(const std::function<void(boost::optional<std::tuple<uint64_t, bool>>)>& handler) {
        if(m_client) {
            COCAINE_LOG_DEBUG(m_logger, "Sending vote request.");
            m_client->call<typename io::raft<typename log_type::value_type>::request_vote>(
                std::make_shared<detail::raft_dispatch>(
                    m_local.service().context(),
                    m_id.first,
                    handler
                ),
                m_local.name(),
                m_local.current_term(),
                m_local.id(),
                std::make_tuple(m_local.log().last_index(), m_local.log().last_term())
            );
        } else {
            COCAINE_LOG_DEBUG(m_logger, "Client disconnected. Unable to send vote request.");
            handler(boost::none);
        }
    }

    void
    send_append() {
        if(m_client && m_local.is_leader()) {
            auto handler = std::bind(&append_handler_t::handle, m_append_state, std::placeholders::_1);
            auto dispatch = std::make_shared<detail::raft_dispatch>(
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
            COCAINE_LOG_DEBUG(m_logger,
                              "Sending append request; term %d; next %d; last %d.",
                              m_local.current_term(),
                              m_next_index,
                              m_local.log().last_index());
        } else {
            COCAINE_LOG_DEBUG(m_logger, "Client disconnected or the local node is not the leader. Unable to send append request.");
            m_append_state->handle(boost::none);
        }
    }

    void
    send_heartbeat() {
        if(m_client) {
            //COCAINE_LOG_DEBUG(m_logger, "Sending heartbeat.");
            m_client->call<typename io::raft<typename log_type::value_type>::append>(
                std::shared_ptr<io::dispatch_t>(),
                m_local.name(),
                m_local.current_term(),
                m_local.id(),
                std::make_tuple(0, 0),
                std::vector<typename log_type::value_type>(),
                m_local.commit_index()
            );
        }
    }

    void
    heartbeat(ev::timer&, int) {
        if(m_local.is_leader()) {
            if(m_append_state || m_local.log().last_index() < m_next_index) {
                ensure_connection(std::bind(&remote_node::send_heartbeat, this));
            } else {
                replicate();
            }
        }
    }

    void
    ensure_connection(const std::function<void()>& handler) {
        if(m_client) {
            //COCAINE_LOG_DEBUG(m_logger, "Client is connected. Call handler.");
            handler();
        } else {
            COCAINE_LOG_DEBUG(m_logger, "Client is not connected. Connecting...");

            std::shared_ptr<client_t> locator = make_client(m_id.first, m_id.second);

            if(!locator) {
                COCAINE_LOG_DEBUG(m_logger, "Unable to connect to locator.");
                handler();
                return;
            }

            m_resolve_state = std::make_shared<resolve_handler_t>(locator, *this, handler);

            auto dispatch = std::make_shared<detail::resolve_dispatch>(
                m_local.service().context(),
                std::string(),
                std::bind(&resolve_handler_t::handle, m_resolve_state, std::placeholders::_1)
            );

            locator->call<cocaine::io::locator::resolve>(dispatch, "raft");
        }
    }

    void
    connect(const std::string& host, uint16_t port) {
        m_client = make_client(host, port);
    }

    std::shared_ptr<client_t>
    make_client(const std::string& host, uint16_t port) {
        auto endpoints = io::resolver<io::tcp>::query(host, port);

        for(auto it = endpoints.begin(); it != endpoints.end(); ++it) {
            try {
                auto socket = std::make_shared<io::socket<io::tcp>>(*it);
                auto channel = std::make_unique<io::channel<io::socket<io::tcp>>>(
                    m_local.service().reactor(),
                    socket
                );
                return std::make_shared<client_t>(std::move(channel));
            } catch(const std::exception&) {
                // Ignore.
            }
        }

        return std::shared_ptr<client_t>();
    }

    void
    on_error(const std::error_code& ec) {
        COCAINE_LOG_INFO(m_logger, "Connection error: %s.", ec.message());
        reset();
    }

private:
    const std::unique_ptr<logging::log_t> m_logger;

    node_id_t m_id;

    actor_type &m_local;

    std::shared_ptr<client_t> m_client;

    ev::timer m_heartbeat_timer;

    std::shared_ptr<resolve_handler_t> m_resolve_state;

    std::shared_ptr<append_handler_t> m_append_state;

    // The next log entry to send to the follower.
    uint64_t m_next_index;

    // The last entry replicated to the follower.
    uint64_t m_match_index;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_REMOTE_HPP
