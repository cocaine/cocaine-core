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

#include "cocaine/traits.hpp"

using namespace cocaine::io;

class msgpack_category_t:
    public std::error_category
{
    virtual
    const char*
    name() const throw() {
        return "msgpack";
    }

    virtual
    std::string
    message(int code) const {
        switch(code) {
            case msgpack_errc::unpack_extra_bytes:
                return "extra bytes in the buffer";

            case msgpack_errc::unpack_continue:
                return "insufficient bytes in the buffer";

            case msgpack_errc::unpack_parse_error:
                return "invalid bytes in the buffer";

            default:
                return "unknown error";
        }
    }
};

static msgpack_category_t category_impl;

namespace cocaine { namespace io {

const std::error_category&
msgpack_category() {
    return category_impl;
}

std::error_code
make_error_code(msgpack_errc e) {
    return std::error_code(static_cast<int>(e), msgpack_category());
}

std::error_condition
make_error_condition(msgpack_errc e) {
    return std::error_condition(static_cast<int>(e), msgpack_category());
}

}} // namespace cocaine::io
