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

#ifndef COCAINE_TYPE_TRAITS_HPP
#define COCAINE_TYPE_TRAITS_HPP

#include <type_traits>

#include <boost/mpl/begin.hpp>
#include <boost/mpl/deref.hpp>
#include <boost/mpl/next.hpp>
#include <boost/mpl/size.hpp>

#include <msgpack.hpp>

namespace cocaine { namespace io {

template<class T>
struct type_traits {
    template<class Stream>
    static
    void
    pack(msgpack::packer<Stream>& packer,
         const T& object)
    {
        packer << object;
    }
    
    static
    void
    unpack(const msgpack::object& packed,
           T& object)
    {
        packed >> object;
    }
};

namespace detail {
    template<class T, class Stream>
    static inline
    void
    pack_sequence(msgpack::packer<Stream>&) {
        return;
    }

    template<class T, class Stream, class Head, typename... Tail>
    static inline
    void
    pack_sequence(msgpack::packer<Stream>& packer,
                  const Head& head,
                  const Tail&... tail)
    {
        // Strip the type.
        typedef typename std::remove_const<
            typename std::remove_reference<Head>::type
        >::type type;

        static_assert(
            std::is_same<typename boost::mpl::deref<T>::type, type>::value,
            "sequence element type mismatch"
        );

        // Pack the current element using the correct packer.
        type_traits<type>::pack(packer, head);

        // Recurse to the next element.
        return pack_sequence<typename boost::mpl::next<T>::type>(
            packer,
            tail...
        );
    }

    template<class T>
    static inline
    void
    unpack_sequence(const msgpack::object*) {
        return;
    }

    template<class T, class Head, typename... Tail>
    static inline
    void
    unpack_sequence(const msgpack::object * unpacked,
                    Head& head,
                    Tail&... tail)
    {
        // Strip the type.
        typedef typename std::remove_const<
            typename std::remove_reference<Head>::type
        >::type type;

        static_assert(
            std::is_same<typename boost::mpl::deref<T>::type, type>::value,
            "sequence element type mismatch"
        );

        // Unpack the current element using the correct packer.
        type_traits<type>::unpack(*unpacked, head);

        // Recurse to the next element.
        return unpack_sequence<typename boost::mpl::next<T>::type>(
            ++unpacked,
            tail...
        );
    }
}

template<class T, class Stream, typename... Args>
static inline
void
pack_sequence(Stream& stream,
              const Args&... sequence)
{
    static_assert(
        sizeof...(sequence) == boost::mpl::size<T>::value,
        "sequence size mismatch"
    );

    msgpack::packer<Stream> packer(stream);

    // The sequence will be packed as an array.
    packer.pack_array(sizeof...(sequence));

    // Recursively pack every sequence element.
    detail::pack_sequence<typename boost::mpl::begin<T>::type>(
        packer,
        sequence...
    );
}

template<class T, typename... Args>
static inline
void
unpack_sequence(const msgpack::object& packed,
                Args&... sequence)
{
    static_assert(
        sizeof...(sequence) == boost::mpl::size<T>::value,
        "sequence size mismatch"
    );

    if(packed.type != msgpack::type::ARRAY ||
       packed.via.array.size != sizeof...(sequence))
    {
        throw msgpack::type_error();
    }

    // Recursively unpack every tuple element while validating the types.
    detail::unpack_sequence<typename boost::mpl::begin<T>::type>(
        packed.via.array.ptr,
        sequence...
    );
}

}} // namespace cocaine::io

#endif
