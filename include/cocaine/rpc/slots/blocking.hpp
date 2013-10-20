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

#ifndef COCAINE_IO_BLOCKING_SLOT_HPP
#define COCAINE_IO_BLOCKING_SLOT_HPP

#include "cocaine/rpc/slots/function.hpp"

namespace cocaine { namespace io {

// Blocking slot

template<class R, class Event, class Sequence = typename io::event_traits<Event>::tuple_type>
struct blocking_slot:
    public function_slot<R, Sequence>
{
    typedef function_slot<R, Sequence> parent_type;
    typedef typename parent_type::callable_type callable_type;

    blocking_slot(callable_type callable):
        parent_type(Event::alias(), callable),
        m_packer(m_buffer)
    { }

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
        type_traits<R>::pack(m_packer, this->call(unpacked));

        upstream->write(m_buffer.data(), m_buffer.size());
        upstream->close();

        m_buffer.clear();

        // Return an empty protocol dispatch.
        return std::shared_ptr<dispatch_t>();
    }

private:
    msgpack::sbuffer m_buffer;
    msgpack::packer<msgpack::sbuffer> m_packer;
};

// Blocking slot specialization for void functions

template<class Event, class Sequence>
struct blocking_slot<void, Event, Sequence>:
    public function_slot<void, Sequence>
{
    typedef function_slot<void, Sequence> parent_type;
    typedef typename parent_type::callable_type callable_type;

    blocking_slot(callable_type callable):
        parent_type(Event::alias(), callable)
    { }

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
        this->call(unpacked);

        // This is needed so that service clients could detect operation completion.
        upstream->close();

        // Return an empty protocol dispatch.
        return std::shared_ptr<dispatch_t>();
    }
};

}} // namespace cocaine::io

#endif
