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

#include <system_error>

#include <boost/asio/io_service.hpp>
#include <boost/lexical_cast.hpp>

namespace cocaine { namespace io {

class gai_category_t:
    public std::error_category
{
    virtual
    const char*
    name() const throw() {
        return "getaddrinfo";
    }

    virtual
    std::string
    message(int code) const {
        return gai_strerror(code);
    }
};

inline
const std::error_category&
gai_category() {
    static gai_category_t category_instance;
    return category_instance;
}

template<class Medium>
struct resolver {
    typedef typename Medium::endpoint endpoint_type;
    typedef typename Medium::resolver resolver_type;

    static inline
    std::vector<endpoint_type>
    query(const std::string& name, uint16_t port) {
        return query(typename resolver_type::query(
            name,
            boost::lexical_cast<std::string>(port)
        ));
    }

    static inline
    std::vector<endpoint_type>
    query(typename endpoint_type::protocol_type protocol, const std::string& name, uint16_t port) {
        return query(typename resolver_type::query(
            protocol,
            name,
            boost::lexical_cast<std::string>(port)
        ));
    }

private:
    static inline
    std::vector<endpoint_type>
    query(typename resolver_type::query query) {
        boost::asio::io_service service;
        resolver_type resolver(service);

        auto it = typename resolver_type::iterator(),
             end = it;

        try {
            it = resolver.resolve(query);
        } catch(const boost::system::system_error& e) {
            throw std::system_error(e.code().value(), gai_category());
        }

        return std::vector<endpoint_type>(it, end);
    }
};

}} // namespace cocaine::io

#endif
