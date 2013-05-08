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

#ifndef COCAINE_SERIALIZATION_TRAITS_HPP
#define COCAINE_SERIALIZATION_TRAITS_HPP

#include <type_traits>

#include <boost/mpl/begin.hpp>
#include <boost/mpl/count_if.hpp>
#include <boost/mpl/deref.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/next.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/transform.hpp>

#include <msgpack.hpp>

#include "cocaine/rpc/optional.hpp"

#include "cocaine/tuples.hpp"

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

// NOTE: This magic specialization allows to pack string literals. Unpacking is intentionally
// prohibited as it might force us to silently drop characters if the buffer is not long enough.

template<size_t N>
struct type_traits<char[N]> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const char* source) {
        packer.pack_raw(N);
        packer.pack_raw_body(source, N);
    }
};

// NOTE: This another magic specialization allows to pack enumerations. On older compilers it will
// assume that the underlying type is int, on GCC 4.7 and Clang it can detect the underlying type.

template<class T>
struct type_traits<
    T,
    typename std::enable_if<std::is_enum<T>::value>::type
>
{
#if defined(__clang__) || defined(GCC47)
    typedef typename std::underlying_type<T>::type base_type;
#else
    typedef int base_type;
#endif

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const T& source) {
        packer << static_cast<base_type>(source);
    }

    static inline
    void
    unpack(const msgpack::object& unpacked, T& target) {
        target = static_cast<T>(unpacked.as<base_type>());
    }
};

// NOTE: The following structure is a template specialization for type lists, to support validating
// sequence packing and unpacking, which can be used as follows:
//
// type_traits<Sequence>::pack(buffer, std::forward<Args>(args)...);
// type_traits<Sequence>::unpack(object, std::forward<Args>(args)...);
//
// It might be a better idea to do that via the type_traits<Args...> template, but such kind of
// template argument pack expanding is not yet supported by GCC 4.4, which we use on Ubuntu Lucid.

template<class T>
struct type_traits<
    T,
    typename std::enable_if<boost::mpl::is_sequence<T>::value>::type
>
{
    template<class Stream, typename... Args>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const Args&... sequence) {
        const size_t minimal = boost::mpl::count_if<
            T,
            boost::mpl::lambda<detail::is_required<boost::mpl::arg<1>>>
        >::value;

        static_assert(sizeof...(sequence) >= minimal, "sequence length mismatch");

        // The sequence will be packed as an array.
        packer.pack_array(sizeof...(sequence));

        typedef typename boost::mpl::transform<
            T,
            boost::mpl::lambda<detail::unwrap_type<boost::mpl::arg<1>>>
        >::type sequence_type;

        // Recursively pack every sequence element.
        pack_sequence<typename boost::mpl::begin<sequence_type>::type>(packer, sequence...);
    }

    template<typename... Args>
    static inline
    void
    unpack(const msgpack::object& object, Args&... sequence) {
        const size_t size = boost::mpl::size<T>::value;

        static_assert(sizeof...(sequence) == size, "sequence length mismatch");

        if(object.type != msgpack::type::ARRAY || object.via.array.size != size) {
            throw msgpack::type_error();
        }

        typedef typename boost::mpl::transform<
            T,
            boost::mpl::lambda<detail::unwrap_type<boost::mpl::arg<1>>>
        >::type sequence_type;

        // Recursively unpack every tuple element while validating the types.
        unpack_sequence<typename boost::mpl::begin<sequence_type>::type>(object.via.array.ptr, sequence...);
    }

private:
    template<class It, class Stream>
    static inline
    void
    pack_sequence(msgpack::packer<Stream>& /* packer */) {
        return;
    }

    template<class It, class Stream, class Head, typename... Tail>
    static inline
    void
    pack_sequence(msgpack::packer<Stream>& packer, const Head& head, const Tail&... tail) {
        // Strip the type.
        typedef typename std::remove_const<
            typename std::remove_reference<Head>::type
        >::type type;

        static_assert(
            std::is_convertible<type, typename boost::mpl::deref<It>::type>::value,
            "sequence element type mismatch"
        );

        // Pack the current element using the correct packer.
        type_traits<type>::pack(packer, head);

        // Recurse to the next element.
        return pack_sequence<typename boost::mpl::next<It>::type>(packer, tail...);
    }

    template<class It>
    static inline
    void
    unpack_sequence(const msgpack::object* /* packed */) {
        return;
    }

    template<class It, class Head, typename... Tail>
    static inline
    void
    unpack_sequence(const msgpack::object* packed, Head& head, Tail&... tail) {
        // Strip the type.
        typedef typename std::remove_const<
            typename std::remove_reference<Head>::type
        >::type type;

        static_assert(
            std::is_convertible<type, typename boost::mpl::deref<It>::type>::value,
            "sequence element type mismatch"
        );

        // Unpack the current element using the correct packer.
        type_traits<type>::unpack(*packed, head);

        // Recurse to the next element.
        return unpack_sequence<typename boost::mpl::next<It>::type>(++packed, tail...);
    }
};

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

#endif
