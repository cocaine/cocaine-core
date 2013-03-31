/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_TYPE_TRAITS_HPP
#define COCAINE_TYPE_TRAITS_HPP

#include <type_traits>

#include <boost/mpl/begin.hpp>
#include <boost/mpl/deque.hpp>
#include <boost/mpl/deref.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/next.hpp>
#include <boost/mpl/push_back.hpp>
#include <boost/mpl/size.hpp>

#include <msgpack.hpp>

namespace cocaine { namespace io {

template<class T, class = void>
struct type_traits {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer,
         const T& source)
    {
        packer << source;
    }

    static inline
    void
    unpack(const msgpack::object& unpacked,
           T& target)
    {
        unpacked >> target;
    }
};

// NOTE: This magic specialization allows to pack string literals. Unpacking
// is intentionally prohibited as it might force us to silently drop characters
// if the buffer is not long enough.

template<int N>
struct type_traits<char[N]> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer,
         const char * source)
    {
        packer.pack_raw(N);
        packer.pack_raw_body(source, N);
    }
};

// NOTE: The following structure is a template specialization for type lists,
// to support validating sequence packing and unpacking, which can be used as
// follows:
//
// type_traits<Sequence>::pack(buffer, std::forward<Args>(args)...);
// type_traits<Sequence>::unpack(object, std::forward<Args>(args)...);
//
// It might be a better idea to do that via the type_traits<Args...> template,
// but such kind of template argument pack expanding is not yet supported by
// GCC 4.4, which we use on Ubuntu Lucid.

template<class T>
struct type_traits<
    T,
    typename std::enable_if<boost::mpl::is_sequence<T>::value>::type
>
{
    template<class Stream, typename... Args>
    static inline
    void
    pack(msgpack::packer<Stream>& packer,
         const Args&... sequence)
    {
        const size_t size = boost::mpl::size<T>::value;

        static_assert(
            sizeof...(sequence) == size,
            "sequence length mismatch"
        );

        // The sequence will be packed as an array.
        packer.pack_array(size);

        // Recursively pack every sequence element.
        pack_sequence<typename boost::mpl::begin<T>::type>(
            packer,
            sequence...
        );
    }

    template<typename... Args>
    static inline
    void
    unpack(const msgpack::object& object,
           Args&... sequence)
    {
        const size_t size = boost::mpl::size<T>::value;

        static_assert(
            sizeof...(sequence) == size,
            "sequence length mismatch"
        );

        if(object.type != msgpack::type::ARRAY ||
           object.via.array.size != size)
        {
            throw msgpack::type_error();
        }

        // Recursively unpack every tuple element while validating the types.
        unpack_sequence<typename boost::mpl::begin<T>::type>(
            object.via.array.ptr,
            sequence...
        );
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
    pack_sequence(msgpack::packer<Stream>& packer,
                  const Head& head,
                  const Tail&... tail)
    {
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
        return pack_sequence<typename boost::mpl::next<It>::type>(
            packer,
            tail...
        );
    }

    template<class It>
    static inline
    void
    unpack_sequence(const msgpack::object * /* packed */) {
        return;
    }

    template<class It, class Head, typename... Tail>
    static inline
    void
    unpack_sequence(const msgpack::object * packed,
                    Head& head,
                    Tail&... tail)
    {
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
        return unpack_sequence<typename boost::mpl::next<It>::type>(
            ++packed,
            tail...
        );
    }
};

namespace detail {
    template<class It, class End, typename... Args>
    struct fold_impl {
        typedef typename fold_impl<
            typename boost::mpl::next<It>::type,
            End,
            Args...,
            typename boost::mpl::deref<It>::type
        >::type type;
    };

    template<class End, typename... Args>
    struct fold_impl<End, End, Args...> {
        typedef std::tuple<Args...> type;
    };

    template<class, typename...>
    struct unfold_impl;

    template<class TypeList, class Head, typename... Args>
    struct unfold_impl<TypeList, Head, Args...> {
        typedef typename unfold_impl<
            typename boost::mpl::push_back<TypeList, Head>::type,
            Args...
        >::type type;
    };

    template<class TypeList>
    struct unfold_impl<TypeList> {
        typedef TypeList type;
    };

    template<int... Indexes>
    struct splat_impl {
        template<class TypeList, class Stream, typename... Args>
        static inline
        void
        pack(msgpack::packer<Stream>& packer,
             const std::tuple<Args...>& source)
        {
            type_traits<TypeList>::pack(packer, std::get<Indexes>(source)...);
        }

        template<class TypeList, typename... Args>
        static inline
        void
        unpack(const msgpack::object& unpacked,
               std::tuple<Args...>& target)
        {
            type_traits<TypeList>::unpack(unpacked, std::get<Indexes>(target)...);
        }
    };

    template<int N, int... Indexes>
    struct splat {
        typedef typename splat<
            N - 1,
            N - 1,
            Indexes...
        >::type type;
    };

    template<int... Indexes>
    struct splat<0, Indexes...> {
        typedef splat_impl<Indexes...> type;
    };
}

template<typename TypeList>
struct fold {
    typedef typename detail::fold_impl<
        typename boost::mpl::begin<TypeList>::type,
        typename boost::mpl::end<TypeList>::type
    >::type type;
};

template<typename... Args>
struct unfold {
    typedef typename detail::unfold_impl<
        boost::mpl::deque<>,
        Args...
    >::type type;
};

template<typename... Args>
struct type_traits<std::tuple<Args...>> {
    typedef typename unfold<Args...>::type sequence_type;
    typedef typename detail::splat<sizeof...(Args)>::type invoking_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& packer,
         const std::tuple<Args...>& source)
    {
        invoking_type::template pack<sequence_type>(packer, source);
    }

    static inline
    void
    unpack(const msgpack::object& unpacked,
           std::tuple<Args...>& target)
    {
        invoking_type::template unpack<sequence_type>(unpacked, target);
    }
};

}} // namespace cocaine::io

#endif
