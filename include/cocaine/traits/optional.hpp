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

#ifndef COCAINE_IO_OPTIONAL_SERIALIZATION_TRAITS_HPP
#define COCAINE_IO_OPTIONAL_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"

#include <boost/optional/optional.hpp>

namespace cocaine { namespace io {

template<class T>
struct type_traits<boost::optional<T>> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const boost::optional<T>& source) {
        if(source) {
            type_traits<T>::pack(target, *source);
        } else {
            target << msgpack::type::nil();
        }
    }

    static inline
    void
    unpack(const msgpack::object& source, boost::optional<T>& target) {
        if(source.type != msgpack::type::NIL) {
            target = T();
            type_traits<T>::unpack(source, *target);
        } else {
            target = boost::none;
        }
    }
};

}} // namespace cocaine::io

#endif
