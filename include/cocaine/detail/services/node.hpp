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

#ifndef COCAINE_NODE_SERVICE_HPP
#define COCAINE_NODE_SERVICE_HPP

#include "cocaine/api/service.hpp"
#include "cocaine/api/storage.hpp"

#include "cocaine/detail/services/node/forwards.hpp"

#include "cocaine/idl/node.hpp"

#include "cocaine/locked_ptr.hpp"

#include "cocaine/rpc/dispatch.hpp"

namespace cocaine { namespace service {

class node_t:
    public api::service_t,
    public dispatch<io::node_tag>
{
    context_t& m_context;

    const std::unique_ptr<logging::log_t> m_log;

    typedef std::map<
        std::string,
        std::shared_ptr<app_t>
    > app_map_t;

    // Apps.
    synchronized<app_map_t> m_apps;

public:
    node_t(context_t& context, io::reactor_t& reactor, const std::string& name, const dynamic_t& args);

    virtual
   ~node_t();

    virtual
    auto
    prototype() -> io::basic_dispatch_t&;

private:
    dynamic_t
    on_start_app(const std::map<std::string, std::string>& runlist);

    dynamic_t
    on_pause_app(const std::vector<std::string>& applist);

    dynamic_t
    on_list() const;
};

}} // namespace cocaine::service

#endif
