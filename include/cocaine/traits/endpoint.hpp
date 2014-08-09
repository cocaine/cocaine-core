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

#ifndef COCAINE_IO_ENDPOINT_SERIALIZATION_TRAITS_HPP
#define COCAINE_IO_ENDPOINT_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"
#include "cocaine/traits/tuple.hpp"

#include <boost/asio/ip/basic_endpoint.hpp>

namespace cocaine { namespace io {

template<class InternetProtocol>
struct type_traits<boost::asio::ip::basic_endpoint<InternetProtocol>> {
    typedef boost::asio::ip::basic_endpoint<InternetProtocol> endpoint_type;
    typedef std::tuple<std::string, unsigned short> tuple_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const endpoint_type& source) {
        const std::string address = source.address().to_string();
        const unsigned short port = source.port();

        type_traits<tuple_type>::pack(target, tuple_type(address, port));
    }

    static inline
    void
    unpack(const msgpack::object& source, endpoint_type& target) {
        std::string address;
        unsigned short port;

        type_traits<tuple_type>::unpack(source, std::move(std::tie(address, port)));

        target.address(boost::asio::ip::address::from_string(address));
        target.port(port);
    }
};

}} // namespace cocaine::io

#endif
