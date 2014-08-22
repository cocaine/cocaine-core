/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_IO_ERROR_CODES_HPP
#define COCAINE_IO_ERROR_CODES_HPP

#include <boost/system/error_code.hpp>

namespace cocaine { namespace error {

enum decode_errors {
    parse_error = 1,
    frame_format_error,
    insufficient_bytes
};

namespace aux {

class decode_category_t:
    public boost::system::error_category
{
    virtual
    const char*
    name() const throw() {
        return "cocaine.rpc.asio";
    }

    virtual
    std::string
    message(int code) const {
        switch(code) {
          case decode_errors::parse_error:
            return "unable to parse the incoming data";

          case decode_errors::frame_format_error:
            return "message has an unexpected framing";

          case decode_errors::insufficient_bytes:
            return "insufficient bytes provided to decode the message";
        }

        return "cocaine.rpc.asio error";
    }
};

} // namespace aux

inline
const boost::system::error_category&
decode_category() {
    static aux::decode_category_t instance;
    return instance;
}

inline
boost::system::error_code
make_error_code(decode_errors code) {
    return boost::system::error_code(static_cast<int>(code), decode_category());
}

}} // namespace cocaine::error

namespace boost { namespace system {

template<>
struct is_error_code_enum<cocaine::error::decode_errors>:
    public true_type
{ };

}} // namespace boost::system

#endif
