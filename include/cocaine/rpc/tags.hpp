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

#ifndef COCAINE_IO_TAGS_HPP
#define COCAINE_IO_TAGS_HPP

#include "cocaine/utility.hpp"

#include <type_traits>

namespace cocaine { namespace io {

// Value tags

template<class T>
struct optional;

template<class T, T Default>
struct optional_with_default;

// Protocol tags

struct recursive_tag;

template<class T>
struct streaming_tag;

template<typename... Types>
struct stream_of {
    typedef streaming_tag<typename itemize<Types...>::type> tag;
};

namespace detail {

template<class T>
struct is_required:
    public std::true_type
{ };

template<class T>
struct is_required<optional<T>>:
    public std::false_type
{ };

template<class T, T Default>
struct is_required<optional_with_default<T, Default>>:
    public std::false_type
{ };

template<class T>
struct unwrap_type {
    typedef T type;
};

template<class T>
struct unwrap_type<optional<T>> {
    typedef T type;
};

template<class T, T Default>
struct unwrap_type<optional_with_default<T, Default>> {
    typedef T type;
};

}}} // namespace cocaine::io::detail

#endif
