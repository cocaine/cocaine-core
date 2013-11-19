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

#ifndef COCAINE_UTILITY_HPP
#define COCAINE_UTILITY_HPP

#include <type_traits>

#if !defined(__clang__) && !defined(HAVE_GCC46)
    // GCC 4.4 defines std::result_of<T> there.
    #include <functional>
#endif

#include <boost/mpl/deque.hpp>
#include <boost/mpl/push_back.hpp>

namespace cocaine {

template<class T>
struct depend {
    typedef void type;
};

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

template<class T>
struct pristine {
    typedef typename std::remove_cv<
        typename std::remove_reference<T>::type
    >::type type;
};

template<class F, class = void>
struct result_of {
    typedef typename std::result_of<F>::type type;
};

template<class F>
struct result_of<F, typename depend<typename F::result_type>::type> {
    typedef typename F::result_type type;
};

} // namespace cocaine

#endif
