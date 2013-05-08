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

#ifndef COCAINE_IO_RESOLVER_HPP
#define COCAINE_IO_RESOLVER_HPP

#include "cocaine/common.hpp"

#include <cstring>

#include <netdb.h>
#include <netinet/in.h>

namespace cocaine { namespace io {

template<class Medium>
struct resolver {
    typedef Medium medium_type;
    typedef typename medium_type::endpoint endpoint_type;
    typedef typename endpoint_type::size_type size_type;

    static inline
    endpoint_type
    query(const std::string& name) {
        medium_type medium;
        endpoint_type endpoint;

        addrinfo hints, *result = nullptr;

        std::memset(&hints, 0, sizeof(addrinfo));

        hints.ai_family = medium.family();
        hints.ai_socktype = medium.type();
        hints.ai_protocol = medium.protocol();

        int rv = ::getaddrinfo(name.c_str(), nullptr, &hints, &result);

        if(rv != 0) {
            throw cocaine::error_t("unable to resolve the name - %s", gai_strerror(rv));
        }

        if(result == nullptr) {
            throw cocaine::error_t("unable to resolve the name");
        }

        BOOST_ASSERT(endpoint.size() == result->ai_addrlen);

        std::memcpy(endpoint.data(), result->ai_addr, result->ai_addrlen);

        ::freeaddrinfo(result);

        return endpoint;
    }
};

}} // namespace cocaine::io

#endif
