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

#ifndef COCAINE_TUPLE_SERIALIZATION_TRAITS_HPP
#define COCAINE_TUPLE_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"
#include "cocaine/traits/typelist.hpp"

#include "cocaine/tuple.hpp"

namespace cocaine { namespace io {

// Tuple serialization

template<typename... Args>
struct type_traits<std::tuple<Args...>> {
    typedef typename aux::make_tuple_traits<sizeof...(Args)>::type traits_type;
    typedef typename tuple::unfold<Args...>::type sequence_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const std::tuple<Args...>& source) {
        traits_type::template pack<sequence_type>(packer, source);
    }

    static inline
    void
    unpack(const msgpack::object& unpacked, std::tuple<Args...>& target) {
        traits_type::template unpack<sequence_type>(unpacked, target);
    }
};

}} // namespace cocaine::io

namespace msgpack {

template<typename... Args>
inline
std::tuple<Args...>&
operator>>(object o, std::tuple<Args...>& t) {
    cocaine::io::type_traits<std::tuple<Args...>>::unpack(o, t);
    return t;
}

template<class Stream, typename... Args>
inline
packer<Stream>&
operator<<(packer<Stream>& o, const std::tuple<Args...>& t) {
    cocaine::io::type_traits<std::tuple<Args...>>::pack(o, t);
    return o;
}

} // namespace msgpack

#endif
