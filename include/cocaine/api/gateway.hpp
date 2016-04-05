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

#ifndef COCAINE_GATEWAY_API_HPP
#define COCAINE_GATEWAY_API_HPP

#include "cocaine/common.hpp"

#include <asio/ip/tcp.hpp>

namespace cocaine { namespace api {

struct gateway_t {
    typedef gateway_t category_type;

    virtual
   ~gateway_t() {
        // Empty.
    }

    typedef std::tuple<std::string, unsigned int> partition_t;

    virtual
    auto
    resolve(const partition_t& name) const -> std::vector<asio::ip::tcp::endpoint> = 0;

    virtual
    size_t
    consume(const std::string& uuid,
            const partition_t& name, const std::vector<asio::ip::tcp::endpoint>& endpoints) = 0;

    virtual
    size_t
    cleanup(const std::string& uuid, const partition_t& name) = 0;

protected:
    gateway_t(context_t&, const std::string& /* name */, const dynamic_t& /* args */) {
        // Empty.
    }
};

typedef std::unique_ptr<gateway_t> gateway_ptr;

}} // namespace cocaine::api

#endif
