/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_ADHOC_GATEWAY_HPP
#define COCAINE_ADHOC_GATEWAY_HPP

#include "cocaine/api/gateway.hpp"

#include <random>

namespace cocaine { namespace gateway {

class adhoc_t:
    public api::gateway_t
{
    const std::unique_ptr<logging::log_t> m_log;

    // Used in resolve() method, which is const.
    std::default_random_engine mutable m_random_generator;

    struct remote_t {
        std::string uuid;
        std::vector<asio::ip::tcp::endpoint> endpoints;
    };

    typedef std::multimap<partition_t, remote_t> remote_map_t;

    // TODO: Make sure that remote service metadata is consistent across the whole cluster.
    synchronized<remote_map_t> m_remotes;

public:
    adhoc_t(context_t& context, const std::string& name, const dynamic_t& args);

    virtual
   ~adhoc_t();

    virtual
    auto
    resolve(const partition_t& name) const -> std::vector<asio::ip::tcp::endpoint>;

    virtual
    size_t
    consume(const std::string& uuid,
            const partition_t& name, const std::vector<asio::ip::tcp::endpoint>& endpoints);

    virtual
    size_t
    cleanup(const std::string& uuid, const partition_t& name);
};

}} // namespace cocaine::gateway

#endif
