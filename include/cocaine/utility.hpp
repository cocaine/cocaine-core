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

namespace cocaine {

template<class T>
struct depend {
    typedef void type;
};

template<class F, class = void>
struct result_of {
    typedef typename std::result_of<F>::type type;
};

template<class F>
struct result_of<
    F,
    typename depend<typename F::result_type>::type
>
{
    typedef typename F::result_type type;
};

template<class T>
struct pristine {
    typedef typename std::remove_cv<
        typename std::remove_reference<T>::type
    >::type type;
};

} // namespace cocaine

#endif
