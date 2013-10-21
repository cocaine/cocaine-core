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

struct locator_t::synchronize_slot_t:
    public io::basic_slot<io::locator::synchronize>
{
    synchronize_slot_t(locator_t& self);

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream);

    void
    announce();

    void
    shutdown();

private:
    bool
    dump(const api::stream_ptr_t& upstream);

private:
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> packer;

    locator_t& self;

    std::vector<api::stream_ptr_t> upstreams;
};

locator_t::synchronize_slot_t::synchronize_slot_t(locator_t& self_):
    packer(buffer),
    self(self_)
{ }

std::shared_ptr<dispatch_t>
locator_t::synchronize_slot_t::operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
    io::detail::invoke<io::event_traits<io::locator::synchronize>::tuple_type>::apply(
        std::bind(&synchronize_slot_t::dump, this, upstream),
        unpacked
    );

    // Save this upstream for the future notifications.
    upstreams.push_back(upstream);

    // Return an empty protocol dispatch.
    return std::shared_ptr<dispatch_t>();
}

void
locator_t::synchronize_slot_t::announce() {
    auto disconnected = std::partition(
        upstreams.begin(),
        upstreams.end(),
        std::bind(&synchronize_slot_t::dump, this, _1)
    );

    upstreams.erase(disconnected, upstreams.end());
}

void
locator_t::synchronize_slot_t::shutdown() {
    std::for_each(
        upstreams.begin(),
        upstreams.end(),
        std::bind(&api::stream_t::close, _1)
    );

    upstreams.clear();
}

bool
locator_t::synchronize_slot_t::dump(const api::stream_ptr_t& upstream) {
    buffer.clear();

    io::type_traits<synchronize_result_type>::pack(
        packer,
        self.dump()
    );

    upstream->write(buffer.data(), buffer.size());

    return true;
}
