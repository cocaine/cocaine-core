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

#ifndef COCAINE_IO_VECTOR_SERIALIZATION_TRAITS_HPP
#define COCAINE_IO_VECTOR_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"

#include <vector>

#ifdef __has_include
    #if __has_include(<boost/container/small_vector.hpp>)
        #include <boost/container/small_vector.hpp>
    #endif
#endif

namespace cocaine { namespace io {

template<class T>
struct vector_traits {
    typedef T vector_type;
    typedef typename T::value_type value_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const vector_type& source) {
        target.pack_array(source.size());

        for(auto it = source.begin(); it != source.end(); ++it) {
            type_traits<value_type>::pack(target, *it);
        }
    }

    static inline
    void
    unpack(const msgpack::object& source, vector_type& target) {
        if(source.type != msgpack::type::ARRAY) {
            throw msgpack::type_error();
        }

        target.assign(source.via.array.size, value_type());

        for(size_t i = 0; i < source.via.array.size; ++i) {
            type_traits<value_type>::unpack(source.via.array.ptr[i], target[i]);
        }
    }
};

template<class T>
struct type_traits<std::vector<T>> :
    public vector_traits<std::vector<T>> {};

#ifdef __has_include
#if __has_include(<boost/container/small_vector.hpp>)

template<class T, std::size_t N>
struct type_traits<boost::container::small_vector<T, N>> :
    public vector_traits<boost::container::small_vector<T, N>> {};

#endif
#endif

}} // namespace cocaine::io

#endif
