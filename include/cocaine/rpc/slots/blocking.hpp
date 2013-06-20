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

template<class R, class Sequence>
struct blocking_slot:
    public function_slot<R, Sequence>
{
    typedef function_slot<R, Sequence> base_type;
    typedef typename base_type::callable_type callable_type;

    blocking_slot(const std::string& name, callable_type callable):
        base_type(name, callable),
        m_packer(m_buffer)
    { }

    virtual
    void
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
        type_traits<R>::pack(m_packer, this->call(unpacked));

        upstream->write(m_buffer.data(), m_buffer.size());
        upstream->close();

        m_buffer.clear();
    }

private:
    msgpack::sbuffer m_buffer;
    msgpack::packer<msgpack::sbuffer> m_packer;
};

// Blocking slot specialization for void functions

template<class Sequence>
struct blocking_slot<void, Sequence>:
    public function_slot<void, Sequence>
{
    typedef function_slot<void, Sequence> base_type;
    typedef typename base_type::callable_type callable_type;

    blocking_slot(const std::string& name, callable_type callable):
        base_type(name, callable)
    { }

    virtual
    void
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
        this->call(unpacked);

        // This is needed so that service clients could detect operation completion.
        upstream->close();
    }
};

}} // namespace cocaine::io

#endif
