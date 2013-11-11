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

struct context_t::synchronization_t:
    public basic_slot<io::locator::synchronize>
{
    synchronization_t(context_t& self);

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream);

    void
    announce();

    void
    shutdown();

private:
    void
    dump(const api::stream_ptr_t& upstream);

private:
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer;

    context_t& self;

    std::vector<api::stream_ptr_t> upstreams;
};

context_t::synchronization_t::synchronization_t(context_t& self_):
    packer(buffer),
    self(self_)
{ }

std::shared_ptr<dispatch_t>
context_t::synchronization_t::operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
    io::detail::invoke<event_traits<io::locator::synchronize>::tuple_type>::apply(
        std::bind(&synchronization_t::dump, this, upstream),
        unpacked
    );

    // Save this upstream for the future notifications.
    upstreams.push_back(upstream);

    // Return an empty protocol dispatch.
    return std::shared_ptr<dispatch_t>();
}

void
context_t::synchronization_t::announce() {
    std::for_each(upstreams.begin(), upstreams.end(), std::bind(&synchronization_t::dump, this, _1));
}

void
context_t::synchronization_t::shutdown() {
    std::for_each(upstreams.begin(), upstreams.end(), std::bind(&api::stream_t::close, _1));
}

void
context_t::synchronization_t::dump(const api::stream_ptr_t& upstream) {
    buffer.clear();

    std::lock_guard<std::mutex> guard(self.m_mutex);

    packer.pack_map(self.m_services.size());

    for(auto it = self.m_services.begin(); it != self.m_services.end(); ++it) {
        packer << it->first;
        packer << it->second->metadata();
    }

    upstream->write(buffer.data(), buffer.size());
}
