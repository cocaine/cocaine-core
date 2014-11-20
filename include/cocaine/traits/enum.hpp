/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_ENUM_SERIALIZATION_TRAITS_HPP
#define COCAINE_ENUM_SERIALIZATION_TRAITS_HPP

#include "cocaine/platform.hpp"
#include "cocaine/traits.hpp"

#include <type_traits>

namespace cocaine { namespace io {

// This magic specialization allows to pack enumerations. On older compilers it will assume that
// the underlying type is int, on GCC 4.7 and Clang it can detect the underlying type.

template<class T>
struct type_traits<
    T,
    typename std::enable_if<std::is_enum<T>::value>::type
>
{
#ifdef COCAINE_HAS_FEATURE_UNDERLYING_TYPE
    typedef typename std::underlying_type<T>::type base_type;
#else
    typedef int base_type;
#endif

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const T& source) {
        target << static_cast<base_type>(source);
    }

    static inline
    void
    unpack(const msgpack::object& source, T& target) {
        target = static_cast<T>(source.as<base_type>());
    }
};

}} // namespace cocaine::io

#endif
