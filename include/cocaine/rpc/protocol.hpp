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

#ifndef COCAINE_IO_PROTOCOL_HPP
#define COCAINE_IO_PROTOCOL_HPP

#include <boost/mpl/begin.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/distance.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/size.hpp>

namespace cocaine { namespace io {

namespace mpl = boost::mpl;

template<class Tag>
struct protocol;

namespace detail {
    template<
        class Event,
        class Protocol = typename protocol<typename Event::tag>::type
    >
    struct enumerate:
        public mpl::distance<
            typename mpl::begin<Protocol>::type,
            typename mpl::find<Protocol, Event>::type
        >::type
    {
        static_assert(
            mpl::contains<Protocol, Event>::value,
            "event has not been registered with its protocol"
        );
    };

    template<class T>
    struct depend {
        typedef void type;
    };

    #define DEPENDENT_TYPE(name, default)                       \
        template<class Event, class = void>                     \
        struct name##_type {                                    \
            typedef default type;                               \
        };                                                      \
                                                                \
        template<class Event>                                   \
        struct name##_type<                                     \
            Event,                                              \
            typename depend<typename Event::name##_type>::type  \
        >                                                       \
        {                                                       \
            typedef typename Event::name##_type type;           \
        };

    DEPENDENT_TYPE(tuple, mpl::list<>)
    DEPENDENT_TYPE(result, void)

    #undef DEPENDENT_TYPE
}

template<class Event>
struct event_traits {
    typedef typename detail::tuple_type<Event>::type tuple_type;
    typedef typename detail::result_type<Event>::type result_type;

    enum constants {
        id = detail::enumerate<Event>::value
    };
};

}}

#endif
