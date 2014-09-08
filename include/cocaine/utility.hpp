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

#ifndef COCAINE_UTILITY_HPP
#define COCAINE_UTILITY_HPP

#include <type_traits>

#include <boost/mpl/deque.hpp>
#include <boost/mpl/push_back.hpp>

namespace cocaine {

// Dependent type for SFINAE

template<class T>
struct depend {
    typedef void type;
};

// Variadic pack conversion. Could be done with typdef mpl::list<Args...>, but GCC 4.6 can't compile
// this with a "sorry, not implemented" error message.

namespace aux {

template<class, typename...>
struct itemize_impl;

template<class TypeList, class Head, typename... Args>
struct itemize_impl<TypeList, Head, Args...> {
    typedef typename itemize_impl<
        typename boost::mpl::push_back<TypeList, Head>::type,
        Args...
    >::type type;
};

template<class TypeList>
struct itemize_impl<TypeList> {
    typedef TypeList type;
};

} // namespace aux

template<typename... Args>
struct itemize {
    typedef typename aux::itemize_impl<
        boost::mpl::deque<>,
        Args...
    >::type type;
};

// Type decay. This is a special case of std::decay<T>, which is not removing array extents, because
// it breaks dynamic_t construction and conversion functions.

template<class T>
struct pristine {
    typedef typename std::remove_cv<
        typename std::remove_reference<T>::type
    >::type type;
};

// Result-of type deduction

template<class F, class = void>
struct result_of {
    typedef decltype(std::declval<F>()) type;
};

template<class F>
struct result_of<F, typename depend<typename F::result_type>::type> {
    typedef typename F::result_type type;
};

// Constant index sequences

template<size_t... N>
struct index_sequence {
    static inline
    size_t
    size() {
        return sizeof...(N);
    }
};

namespace aux {

template<size_t I, size_t... Indices>
struct make_index_sequence_impl {
    typedef typename make_index_sequence_impl<I - 1, I - 1, Indices...>::type type;
};

template<size_t... Indices>
struct make_index_sequence_impl<0, Indices...> {
    typedef index_sequence<Indices...> type;
};

} // namespace aux

template<size_t N>
struct make_index_sequence {
    typedef typename aux::make_index_sequence_impl<N>::type type;
};

} // namespace cocaine

#endif
