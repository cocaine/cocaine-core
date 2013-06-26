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

#include "cocaine/rpc/message.hpp"

namespace {

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
            case cocaine::io::rpc_errc::parse_error:
                return "invalid bytes";

            case cocaine::io::rpc_errc::invalid_format:
                return "invalid format";

            default:
                return "unknown error";
        }
    }
};

rpc_category_t category_instance;

}

namespace cocaine { namespace io {

const std::error_category&
rpc_category() {
    return category_instance;
}

std::error_code
make_error_code(rpc_errc e) {
    return std::error_code(static_cast<int>(e), rpc_category());
}

std::error_condition
make_error_condition(rpc_errc e) {
    return std::error_condition(static_cast<int>(e), rpc_category());
}

}} // namespace cocaine::io
