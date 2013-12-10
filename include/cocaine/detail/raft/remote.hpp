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

#include "cocaine/common.hpp"
#include "cocaine/format.hpp"

namespace cocaine {

template<class Actor>
class remote_node {
    typedef Actor actor_type;

    typedef typename actor_type::log_type log_type;

public:
    remote_node(actor_type& local, io::raft::node_id_t id):
        m_id(id),
        m_local(local),
        m_next_index(0),
        m_match_index(0)
    {
        // Empty.
    }

    void
    request_vote(const std::function<void(boost::optional<std::tuple<uint64_t, bool>>)>&,
                 uint64_t term,
                 std:tuple<uint64_t, uint64_t> last_entry)
    {
        ensure_connection();

        m_client->call<io::raft<log_type::value_type>::vote>(
            std::make_shared<result_provider<std::tuple<uint64_t, bool>>>(m_context, m_id.first, handler),
            m_local.name(),
            term,
            m_local.id(),
            last_entry
        );
    }

    void
    replicate(const std::function<void(boost::optional<std::tuple<uint64_t, bool>>)>&,
              uint64_t term,
              std:tuple<uint64_t, uint64_t> last_entry,
              const std::vector<typename Log::value_type>& entries,
              uint64_t commit_index)
    {
        ensure_connection();

        m_client->call<io::raft<log_type::value_type>::append>(
            std::make_shared<result_provider<std::tuple<uint64_t, bool>>>(m_context, m_id.first, handler),
            m_local.name(),
            term,
            m_local.id(),
            last_entry,
            entries,
            commit_index
        );
    }

    uint64_t
    next_index() const {
        return m_next_index;
    }

    uint64_t
    match_index() const {
        return m_match_index;
    }

    void
    reset() {
        m_client.reset();
        m_next_index = 0;
        m_match_index = 0;
    }

private:
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

    void
    on_error(const std::error_code& ec) {
        reset();
    }

private:
    node_id_t m_id;

    actor_type &m_local;

    std::shared_ptr<client_t> m_client;

    // The next log entry to send to the follower.
    uint64_t m_next_index;

    // The last entry replicated to the follower.
    uint64_t m_match_index;
};

} // namespace cocaine

#endif // COCAINE_RAFT_REMOTE_HPP
