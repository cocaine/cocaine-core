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

#ifndef COCAINE_CLUSTER_API_HPP
#define COCAINE_CLUSTER_API_HPP

#include "cocaine/common.hpp"

#include <asio/ip/tcp.hpp>

namespace cocaine { namespace api {

struct cluster_t {
    typedef cluster_t category_type;

    enum class mode_t {
        full,
        announce_only
    };

    struct interface {
        virtual
        auto
        asio() -> asio::io_service& = 0;

        virtual
        void
        link_node(const std::string& uuid, const std::vector<asio::ip::tcp::endpoint>& endpoints) = 0;

        virtual
        void
        drop_node(const std::string& uuid) = 0;

        virtual
        auto
        uuid() const -> std::string = 0;
    };

    virtual
   ~cluster_t() {
        // Empty.
    }

protected:
    cluster_t(context_t&, interface&, mode_t /* mode */ , const std::string& /* name */, const dynamic_t& /* args */) {
        // Empty.
    }
};

typedef std::unique_ptr<cluster_t> cluster_ptr;

}} // namespace cocaine::api

#endif
