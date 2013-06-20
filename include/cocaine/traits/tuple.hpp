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

#ifndef COCAINE_TUPLE_TYPE_TRAITS_HPP
#define COCAINE_TUPLE_TYPE_TRAITS_HPP

#include "cocaine/traits.hpp"

#include "cocaine/tuple.hpp"

namespace cocaine { namespace io {

// Tuple serialization

namespace detail {
    template<size_t... Indices>
    struct splat_impl {
        template<class TypeList, class Stream, typename... Args>
        static inline
        void
        pack(msgpack::packer<Stream>& packer, const std::tuple<Args...>& source) {
            type_traits<TypeList>::pack(packer, std::get<Indices>(source)...);
        }

        template<class TypeList, typename... Args>
        static inline
        void
        unpack(const msgpack::object& unpacked, std::tuple<Args...>& target) {
            type_traits<TypeList>::unpack(unpacked, std::get<Indices>(target)...);
        }
    };

    template<size_t N, size_t... Indices>
    struct splat {
        typedef typename splat<N - 1, N - 1, Indices...>::type type;
    };

    template<size_t... Indices>
    struct splat<0, Indices...> {
        typedef splat_impl<Indices...> type;
    };
}

template<typename... Args>
struct type_traits<std::tuple<Args...>> {
    typedef typename tuple::unfold<Args...>::type sequence_type;
    typedef typename detail::splat<sizeof...(Args)>::type splat_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const std::tuple<Args...>& source) {
        splat_type::template pack<sequence_type>(packer, source);
    }

    static inline
    void
    unpack(const msgpack::object& unpacked, std::tuple<Args...>& target) {
        splat_type::template unpack<sequence_type>(unpacked, target);
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
