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

#ifndef COCAINE_IO_MESSAGE_TYPE_TRAITS_HPP
#define COCAINE_IO_MESSAGE_TYPE_TRAITS_HPP

#include "cocaine/traits.hpp"

#include "cocaine/io.hpp"

namespace cocaine { namespace io {

template<class Event>
struct type_traits<message<Event>> {
    static const size_t length = boost::tuples::length<
        typename event_traits<Event>::tuple_type
    >::value;

    template<class Stream>
    static
    void
    pack(msgpack::packer<Stream>& packer,
         const message<Event>& object)
    {
        packer.pack_array(length);

        // Recursively pack every tuple element.
        pack_tuple(packer, object);
    }

    static
    void
    unpack(const msgpack::object& packed,
           message<Event>& object)
    {
        if(packed.type != msgpack::type::ARRAY ||
           packed.via.array.size != length)
        {
            throw msgpack::type_error();
        }

        // Recursively unpack every tuple element.
        unpack_tuple(packed.via.array.ptr, object);
    }

private:
    template<class Stream>
    static
    void
    pack_tuple(msgpack::packer<Stream>&,
               const null_type&)
    {
        return;
    }
    
    template<class Stream, class Head, class Tail>
    static
    void
    pack_tuple(msgpack::packer<Stream>& packer,
               const cons<Head, Tail>& o)
    {
        // Strip the type.
        typedef typename boost::remove_const<
            typename boost::remove_reference<Head>::type
        >::type type;

        // Pack the current tuple element using the correct packer.
        type_traits<type>::pack(packer, o.get_head());

        // Recurse to the next tuple element.
        return pack_tuple(packer, o.get_tail());
    }

    static
    void
    unpack_tuple(const msgpack::object *,
                 const null_type&)
    {
        return;
    }
    
    template<class Head, class Tail>
    static
    void
    unpack_tuple(const msgpack::object * unpacked,
                 cons<Head, Tail>& o)
    {
        // Strip the type.
        typedef typename boost::remove_const<
            typename boost::remove_reference<Head>::type
        >::type type;

        // Unpack the current tuple element using the correct packer.
        type_traits<type>::unpack(*unpacked, o.get_head());

        // Recurse to the next tuple element.
        return unpack_tuple(++unpacked, o.get_tail());
    }
};

}} // namespace cocaine::io

#endif
