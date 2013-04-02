/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_IO_MESSAGE_HPP
#define COCAINE_IO_MESSAGE_HPP

#include "cocaine/common.hpp"
#include "cocaine/traits.hpp"

#include "cocaine/rpc/protocol.hpp"

namespace cocaine { namespace io {

struct message_t:
    boost::noncopyable
{
    message_t(msgpack::object object):
        m_object(object)
    { }

    template<class Event, typename... Args>
    void
    as(Args&&... targets) const {
        try {
            type_traits<typename event_traits<Event>::tuple_type>::unpack(
                args(),
                std::forward<Args>(targets)...
            );
        } catch(const msgpack::type_error&) {
            throw cocaine::error_t("invalid message type");
        }
    }

public:
    uint32_t
    id() const {
        return m_object.via.array.ptr[0].as<uint32_t>();
    }

    uint64_t
    band() const {
        return m_object.via.array.ptr[1].as<uint64_t>();
    }

    const msgpack::object&
    args() const {
        return m_object.via.array.ptr[2];
    }

private:
    msgpack::object m_object;
};

}}

#endif
