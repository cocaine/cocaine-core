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

#ifndef COCAINE_IO_RESULT_OF_HPP
#define COCAINE_IO_RESULT_OF_HPP

#include "cocaine/rpc/protocol.hpp"

#include "cocaine/tuple.hpp"

#include <boost/mpl/front.hpp>
#include <boost/mpl/size.hpp>

namespace cocaine { namespace io { namespace aux {

template<class T>
struct result_of_impl;

template<class T>
struct result_of_impl<streaming_tag<T>> {
    typedef typename std::conditional<
        boost::mpl::size<T>::value == 1,
        typename boost::mpl::front<T>::type,
        typename tuple::fold<T>::type
    >::type type;
};

template<>
struct result_of_impl<void> {
    typedef void type;
};

}} // namespace io::aux

template<class Event>
struct result_of<Event, typename depend<typename Event::tag>::type> {
    typedef typename io::aux::result_of_impl<
        typename io::event_traits<Event>::drain_type
    >::type type;
};

} // namespace cocaine

#endif
