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

#include <boost/asio/io_service.hpp>
#include <boost/lexical_cast.hpp>

namespace cocaine { namespace io {

template<class Medium>
struct resolver {
    typedef Medium medium_type;
    typedef typename medium_type::endpoint endpoint_type;
    typedef typename medium_type::resolver resolver_type;

    static inline
    std::vector<endpoint_type>
    query(const std::string& name, uint16_t port) {
        boost::asio::io_service service;
        resolver_type resolver(service);

        auto it = typename resolver_type::iterator(),
             end = it;

        try {
            it = resolver.resolve(typename resolver_type::query(
                name,
                boost::lexical_cast<std::string>(port)
            ));
        } catch(const boost::system::system_error& e) {
            throw cocaine::error_t(
                "unable to resolve '%s' - [%d] %s", name, e.code().value(), e.code().message()
            );
        }

        return std::vector<endpoint_type>(it, end);
    }
};

}} // namespace cocaine::io

#endif
