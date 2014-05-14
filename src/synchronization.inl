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

#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/tuple.hpp"

struct context_t::synchronization_t:
    public basic_slot<io::locator::synchronize>
{
    typedef result_of<io::locator::synchronize>::type result_type;

    synchronization_t(context_t& self);

    typedef basic_slot<io::locator::synchronize>::dispatch_type dispatch_type;
    typedef basic_slot<io::locator::synchronize>::tuple_type tuple_type;
    typedef basic_slot<io::locator::synchronize>::upstream_type upstream_type;

    typedef upstream_type::protocol protocol;

    virtual
    std::shared_ptr<dispatch_type>
    operator()(tuple_type&& args, upstream_type&& upstream);

    void
    announce();

    void
    shutdown();

private:
    auto
    dump() const -> result_type;

private:
    context_t& self;

    // Remote clients for future updates.
    std::vector<upstream_type> upstreams;
};

context_t::synchronization_t::synchronization_t(context_t& self_):
    self(self_)
{ }

auto
context_t::synchronization_t::operator()(tuple_type&& /* args */, upstream_type&& upstream)
    -> std::shared_ptr<dispatch_type>
{
    upstream.send<protocol::chunk>(dump());

    // Save this upstream for the future notifications.
    upstreams.emplace_back(std::move(upstream));

    // Return an empty protocol dispatch.
    return std::shared_ptr<dispatch_type>();
}

void
context_t::synchronization_t::announce() {
    result_type result = dump();

    for(auto it = upstreams.begin(); it != upstreams.end(); ++it) {
        it->send<protocol::chunk>(result);
    }
}

void
context_t::synchronization_t::shutdown() {
    for(auto it = upstreams.begin(); it != upstreams.end(); ++it) {
        it->send<protocol::choke>();
    }

    upstreams.clear();
}

auto
context_t::synchronization_t::dump() const -> result_type {
    result_type result;

    auto locked = self.m_services.synchronize();

    for(auto it = locked->begin(); it != locked->end(); ++it) {
        result[it->first] = it->second->metadata();
    }

    return result;
}
