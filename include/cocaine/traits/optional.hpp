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

#ifndef COCAINE_IO_OPTIONAL_SERIALIZATION_TRAITS_HPP
#define COCAINE_IO_OPTIONAL_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"

#include "boost/optional.hpp"

namespace cocaine { namespace io {

template<class T>
struct type_traits<boost::optional<T>> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const boost::optional<T>& source) {
        return source ?
            type_traits<T>::pack(packer, source.get())
          : (void)(packer.pack_nil());
    }

    static inline
    void
    unpack(const msgpack::object& unpacked, boost::optional<T>& target) {
        return unpacked.type != msgpack::type::NIL ?
            type_traits<T>::unpack(unpacked, target.get())
          : (void)(target = boost::none_t());
    }
};

}} // namespace cocaine::io

#endif
