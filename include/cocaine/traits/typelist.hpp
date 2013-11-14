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

#ifndef COCAINE_TYPELIST_SERIALIZATION_TRAITS_HPP
#define COCAINE_TYPELIST_SERIALIZATION_TRAITS_HPP

#include "cocaine/traits.hpp"

#include "cocaine/rpc/protocol.hpp"
#include "cocaine/rpc/tags.hpp"

#include <type_traits>

#include <boost/mpl/begin.hpp>
#include <boost/mpl/count_if.hpp>
#include <boost/mpl/deref.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/next.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/transform.hpp>

namespace cocaine { namespace io {

// NOTE: The following structure is a template specialization for type lists, to support validating
// sequence packing and unpacking, which can be used as follows:
//
// type_traits<Sequence>::pack(buffer, std::forward<Args>(args)...);
// type_traits<Sequence>::unpack(object, std::forward<Args>(args)...);
//
// It might be a better idea to do that via the type_traits<Args...> template, but such kind of
// template argument pack expanding is not yet supported by GCC 4.4, which we use on Ubuntu Lucid.

namespace aux {

// NOTE: This was supposed to be in traits/tuple.hpp, but that would've created a circular include
// dependency, which is not that great at all.

template<size_t... Indices>
struct tuple_traits_impl {
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
struct make_tuple_traits {
    typedef typename make_tuple_traits<N - 1, N - 1, Indices...>::type type;
};

template<size_t... Indices>
struct make_tuple_traits<0, Indices...> {
    typedef tuple_traits_impl<Indices...> type;
};

} // namespace aux

template<class T>
struct type_traits<
    T,
    typename std::enable_if<boost::mpl::is_sequence<T>::value>::type
>
{
    enum {
        minimal = boost::mpl::count_if<
            T,
            boost::mpl::lambda<detail::is_required<boost::mpl::arg<1>>>
        >::value
    };

    typedef typename boost::mpl::transform<
        T,
        boost::mpl::lambda<detail::unwrap_type<boost::mpl::arg<1>>>
    >::type sequence_type;

public:
    template<class Stream, typename... Args>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const Args&... sequence) {
        static_assert(sizeof...(sequence) >= minimal, "sequence length mismatch");

        // The sequence will be packed as an array.
        packer.pack_array(sizeof...(sequence));

        // Recursively pack every sequence element.
        pack_sequence<typename boost::mpl::begin<sequence_type>::type>(packer, sequence...);
    }

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer, const typename tuple::fold<sequence_type>::type& tuple) {
        typedef typename aux::make_tuple_traits<
            boost::mpl::size<sequence_type>::value
        >::type traits_type;

        traits_type::template pack<sequence_type>(packer, tuple);
    }

    template<typename... Args>
    static inline
    void
    unpack(const msgpack::object& object, Args&... sequence) {
        static_assert(sizeof...(sequence) >= minimal, "sequence length mismatch");

        #if defined(__GNUC__) && defined(HAVE_GCC46)
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wtype-limits"
        #endif

        if(object.type != msgpack::type::ARRAY || object.via.array.size < minimal) {
            throw msgpack::type_error();
        }

        #if defined(__GNUC__) && defined(HAVE_GCC46)
            #pragma GCC diagnostic pop
        #endif

        // Recursively unpack every tuple element while validating the types.
        unpack_sequence<typename boost::mpl::begin<sequence_type>::type>(object.via.array.ptr, sequence...);
    }

    static inline
    void
    unpack(const msgpack::object& object, typename tuple::fold<sequence_type>& tuple) {
        typedef typename aux::make_tuple_traits<
            boost::mpl::size<sequence_type>::value
        >::type traits_type;

        traits_type::template unpack<sequence_type>(object, tuple);
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

}} // namespace cocaine::io

#endif
