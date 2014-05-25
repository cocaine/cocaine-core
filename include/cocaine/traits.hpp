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

#ifndef COCAINE_SERIALIZATION_TRAITS_HPP
#define COCAINE_SERIALIZATION_TRAITS_HPP

#include <msgpack.hpp>

namespace cocaine { namespace io {

template<class T, class = void>
struct type_traits {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const T& source) {
        packer << source;
    }

    static inline
    void
    unpack(const msgpack::object& unpacked, T& target) {
        unpacked >> target;
    }
};

}} // namespace cocaine::io

#endif
