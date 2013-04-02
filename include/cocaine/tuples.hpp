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

#ifndef COCAINE_TUPLES_HPP
#define COCAINE_TUPLES_HPP

#include <tuple>

#include <boost/mpl/begin.hpp>
#include <boost/mpl/deque.hpp>
#include <boost/mpl/deref.hpp>
#include <boost/mpl/end.hpp>
#include <boost/mpl/next.hpp>
#include <boost/mpl/push_back.hpp>

namespace cocaine { namespace tuple {

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

}} // namespace cocaine::tuple

#endif
