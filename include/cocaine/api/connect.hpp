/*
    Copyright (c) 2013-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
    Copyright (c) 2014 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_CONNECT_HPP
#define COCAINE_CONNECT_HPP

#include "cocaine/common.hpp"

#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/session.hpp"
#include "cocaine/rpc/upstream.hpp"

#include <asio/ip/tcp.hpp>

namespace cocaine { namespace api {

template<class Tag> class client;

namespace details {

class basic_client_t {
    std::shared_ptr<session_t> m_session;

public:
    template<typename> friend class api::client;

    basic_client_t() = default;
    basic_client_t(basic_client_t&& other);

    virtual
   ~basic_client_t();

    basic_client_t&
    operator=(basic_client_t&& rhs);

    // Observers

    auto
    remote_endpoint() const -> asio::ip::tcp::endpoint;

    // Modifiers

    void
    attach(const std::shared_ptr<session_t>& session);
};

} // namespace details

template<class Tag>
class client:
    public details::basic_client_t
{
    template<class Event, bool = std::is_same<typename Event::tag, Tag>::value>
    struct traits;

    template<class Event>
    struct traits<Event, true> {
        typedef upstream<typename io::event_traits<Event>::dispatch_type>       upstream_type;
        typedef dispatch<typename io::event_traits<Event>::upstream_type> const dispatch_type;
    };

public:
    template<class Event, class... Args>
    typename traits<Event>::upstream_type
    invoke(const std::shared_ptr<typename traits<Event>::dispatch_type>& dispatch, Args&&... args) {
        if(!m_session) {
            throw cocaine::error_t("client is not connected");
        }

        if(std::is_same<typename result_of<Event>::type, io::mute_slot_tag>::value && dispatch) {
            throw cocaine::error_t("callee has no upstreams specified");
        }

        const auto ptr = m_session->fork(dispatch);

        // NOTE: No locking required: session synchronizes channels, hence no races.
        ptr->template send<Event>(std::forward<Args>(args)...);

        return ptr;
    }
};

}} // namespace cocaine::api

#endif
