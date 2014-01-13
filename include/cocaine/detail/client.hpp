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

#ifndef COCAINE_CLIENT_HPP
#define COCAINE_CLIENT_HPP

#include "cocaine/rpc/upstream.hpp"
#include "cocaine/rpc/protocol.hpp"
#include "cocaine/dispatch.hpp"
#include "cocaine/detail/atomic.hpp"

#include <type_traits>
#include <utility>

namespace cocaine {

class client_t {
public:
    typedef cocaine::io::channel<cocaine::io::socket<cocaine::io::tcp>>
            socket_t;

    client_t(std::unique_ptr<socket_t>&& s):
        m_next_channel(1)
    {
        s->rd->bind(std::bind(&client_t::on_message, this, std::placeholders::_1),
                    std::bind(&client_t::on_error, this, std::placeholders::_1));

        s->wr->bind(std::bind(&client_t::on_error, this, std::placeholders::_1));

        m_session = std::make_shared<session_t>(std::move(s));
    }

    ~client_t() {
        m_session->detach();
    }

    void
    bind(std::function<void(const std::error_code&)> error_handler) {
        m_error_handler = error_handler;
    }

    template<class Event, class... Args>
    upstream_t
    call(const std::shared_ptr<io::dispatch_t>& handler, Args&&... args) {
        auto dispatch = std::is_same<typename io::event_traits<Event>::drain_type, void>::value ?
                        std::shared_ptr<io::dispatch_t>() :
                        handler;
        auto upstream = m_session->invoke(m_next_channel++, dispatch);
        upstream->send<Event>(std::forward<Args>(args)...);
        return upstream;
    }

private:
    void
    on_message(const cocaine::io::message_t& message) {
        m_session->invoke(message);
    }

    void
    on_error(const std::error_code& ec) {
        m_session->detach();

        if (m_error_handler) {
            m_error_handler(ec);
        }
    }

private:
    std::shared_ptr<session_t> m_session;
    std::atomic<uint64_t> m_next_channel;

    std::function<void(const std::error_code&)> m_error_handler;
};

} // namespace cocaine

#endif // COCAINE_CLIENT_HPP
