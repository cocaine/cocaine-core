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

#ifndef COCAINE_COUNTER_SERVICE_HPP
#define COCAINE_COUNTER_SERVICE_HPP

#include "cocaine/raft.hpp"

#include "cocaine/api/service.hpp"

#include "cocaine/idl/counter.hpp"
#include "cocaine/rpc/dispatch.hpp"

namespace cocaine { namespace service {

class counter_t:
    public api::service_t,
    public dispatch<io::counter_tag>
{
public:
    struct counter_machine_t;

    typedef raft::actor<counter_machine_t, raft::configuration<counter_machine_t>>
            raft_actor_type;

    counter_t(context_t& context,
              io::reactor_t& reactor,
              const std::string& name,
              const dynamic_t& args);

    virtual
    auto
    prototype() const -> const io::basic_dispatch_t& {
        return *this;
    }

private:
    deferred<raft::command_result<int>>
    on_inc(int value);

    deferred<raft::command_result<int>>
    on_dec(int value);

    deferred<raft::command_result<bool>>
    on_cas(int expected, int desired);

private:
    const std::unique_ptr<logging::log_t> m_log;

    std::shared_ptr<raft_actor_type> m_raft;
};

}} // namespace cocaine::service

#endif // COCAINE_SERVICE_COUNTER_HPP
