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

#ifndef COCAINE_SERVICE_COUNTER_HPP
#define COCAINE_SERVICE_COUNTER_HPP

#include "cocaine/context.hpp"
#include "cocaine/raft.hpp"
#include "cocaine/dispatch.hpp"
#include "cocaine/api/service.hpp"
#include "cocaine/idl/counter.hpp"

namespace cocaine { namespace service {

class counter_t:
    public api::service_t,
    public implements<io::counter_tag>
{
public:
    counter_t(context_t& context,
              io::reactor_t& reactor,
              const std::string& name,
              const dynamic_t& args);

    virtual
    auto
    prototype() -> dispatch_t& {
        return *this;
    }

private:
    deferred<int>
    on_inc(int value);

    deferred<int>
    on_dec(int value);

    deferred<bool>
    on_cas(int expected, int desired);

private:
    const std::unique_ptr<logging::log_t> m_log;

    struct counter_machine_t;

    typedef raft::actor<counter_machine_t, raft::configuration<raft::log<counter_machine_t>>>
            raft_actor_type;

    std::shared_ptr<raft_actor_type> m_raft;
};

}} // namespace cocaine::service

#endif // COCAINE_SERVICE_COUNTER_HPP
