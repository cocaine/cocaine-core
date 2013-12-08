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

#include "cocaine/rpc/protocol.hpp"

#include "cocaine/traits/tuple.hpp"

#include <system_error>

namespace cocaine { namespace io {

enum rpc_errc {
    parse_error,
    frame_format_error,
    data_type_mismatch
};

class rpc_category_t:
    public std::error_category
{
    virtual
    const char*
    name() const throw() {
        return "rpc";
    }

    virtual
    std::string
    message(int code) const {
        switch(code) {
        case rpc_errc::parse_error:
            return "unable to parse the incoming data";

        case rpc_errc::frame_format_error:
            return "message has an unexpected framing";

        case rpc_errc::data_type_mismatch:
            return "message has an unexpected payload";

        default:
            return "unknown error";
        }
    }
};

inline
const std::error_category&
rpc_category() {
    static rpc_category_t category_instance;
    return category_instance;
}

inline
std::error_code
make_error_code(rpc_errc e) {
    return std::error_code(static_cast<int>(e), rpc_category());
}

struct message_t {
    COCAINE_DECLARE_NONCOPYABLE(message_t)

    message_t(const msgpack::object& object):
        m_object(object)
    {
        if(object.type != msgpack::type::ARRAY || object.via.array.size != 3) {
            throw std::system_error(make_error_code(rpc_errc::frame_format_error));
        }

        if(object.via.array.ptr[0].type != msgpack::type::POSITIVE_INTEGER ||
           object.via.array.ptr[1].type != msgpack::type::POSITIVE_INTEGER ||
           object.via.array.ptr[2].type != msgpack::type::ARRAY)
        {
            throw std::system_error(make_error_code(rpc_errc::frame_format_error));
        }
    }

    template<class Event, typename... Args>
    void
    as(Args&&... targets) const {
        try {
            type_traits<typename event_traits<Event>::tuple_type>::unpack(
                args(),
                std::forward<Args>(targets)...
            );
        } catch(const msgpack::type_error& e) {
            throw std::system_error(make_error_code(rpc_errc::data_type_mismatch));
        }
    }

public:
    uint64_t
    band() const {
        return m_object.via.array.ptr[0].as<uint64_t>();
    }

    uint32_t
    id() const {
        return m_object.via.array.ptr[1].as<uint32_t>();
    }

    const msgpack::object&
    args() const {
        return m_object.via.array.ptr[2];
    }

private:
    const msgpack::object& m_object;
};

}} // namespace cocaine::io

namespace std {
    template<>
    struct is_error_code_enum<cocaine::io::rpc_errc>:
        public true_type
    { };
}

#endif
