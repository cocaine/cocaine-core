/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_EVENT_POLICY_TYPE_TRAITS_HPP
#define COCAINE_EVENT_POLICY_TYPE_TRAITS_HPP

#include "cocaine/traits.hpp"

#include "cocaine/api/event.hpp"

namespace cocaine { namespace io {

template<>
struct type_traits<api::policy_t> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer,
         const api::policy_t& source)
    {
        packer.pack_array(3);
        
        packer << source.urgent;
        packer << source.timeout;
        packer << source.deadline;
    }
    
    static inline
    void
    unpack(const msgpack::object& object,
           api::policy_t& target)
    {
        if(object.type != msgpack::type::ARRAY ||
           object.via.array.size != 3)
        {
            throw msgpack::type_error();
        }

        object.via.array.ptr[0] >> target.urgent;
        object.via.array.ptr[1] >> target.timeout;
        object.via.array.ptr[2] >> target.deadline;
    }
};

}} // namespace cocaine::io

#endif
