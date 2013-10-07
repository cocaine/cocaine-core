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

locator_t::synchronize_slot_t::synchronize_slot_t(locator_t& self):
    slot_concept_t("synchronize"),
    m_packer(m_buffer),
    m_self(self)
{ }

void
locator_t::synchronize_slot_t::operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
    io::detail::invoke<io::event_traits<io::locator::synchronize>::tuple_type>::apply(
        std::bind(&synchronize_slot_t::dump, this, upstream),
        unpacked
    );

    // Save this upstream for the future notifications.
    m_upstreams.push_back(upstream);
}

void
locator_t::synchronize_slot_t::update() {
    auto disconnected = std::partition(
        m_upstreams.begin(),
        m_upstreams.end(),
        std::bind(&synchronize_slot_t::dump, this, _1)
    );

    m_upstreams.erase(disconnected, m_upstreams.end());
}

void
locator_t::synchronize_slot_t::shutdown() {
    std::for_each(
        m_upstreams.begin(),
        m_upstreams.end(),
        std::bind(&synchronize_slot_t::close, _1)
    );

    m_upstreams.clear();
}

bool
locator_t::synchronize_slot_t::dump(const api::stream_ptr_t& upstream) {
    m_buffer.clear();

    io::type_traits<synchronize_result_type>::pack(
        m_packer,
        m_self.dump()
    );

    upstream->write(m_buffer.data(), m_buffer.size());

    return true;
}

void
locator_t::synchronize_slot_t::close(const api::stream_ptr_t& upstream) {
    upstream->close();
}
