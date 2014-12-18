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

#ifndef COCAINE_NODE_SERVICE_HPP
#define COCAINE_NODE_SERVICE_HPP

#include "cocaine/api/service.hpp"

#include "cocaine/detail/service/node/forwards.hpp"

#include "cocaine/idl/node.hpp"
#include "cocaine/rpc/dispatch.hpp"

#include "cocaine/locked_ptr.hpp"

namespace cocaine { namespace service {

class node_t;

namespace results {
    typedef result_of<io::node::list>::type list;
}

class node_t:
    public api::service_t,
    public dispatch<io::node_tag>
{
    context_t& m_context;

    const std::unique_ptr<logging::log_t> m_log;

    synchronized<std::map<std::string, std::shared_ptr<app_t>>> m_apps;

public:
    node_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    virtual
   ~node_t();

    virtual
    auto
    prototype() const -> const io::basic_dispatch_t&;

private:
    void
    on_start_app(const std::string& name, const std::string& profile);

    void
    on_pause_app(const std::string& name);

    auto
    on_list() const -> results::list;
};

}} // namespace cocaine::service

#endif
