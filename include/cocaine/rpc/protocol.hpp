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

#ifndef COCAINE_IO_PROTOCOL_HPP
#define COCAINE_IO_PROTOCOL_HPP

#include <boost/mpl/begin.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/distance.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/joint_view.hpp>
#include <boost/mpl/list.hpp>

namespace cocaine { namespace io {

namespace mpl = boost::mpl;

template<class Tag>
struct protocol;

template<class Tag>
struct extends {
    typedef protocol<Tag> parent_type;
};

namespace detail {
    template<class T>
    struct depend {
        typedef void type;
    };

    template<class Protocol, class = void>
    struct flatten {
        typedef typename Protocol::type type;
    };

    template<class Protocol>
    struct flatten<
        Protocol,
        typename depend<typename Protocol::parent_type>::type
    >
    {
        typedef typename mpl::joint_view<
            typename flatten<typename Protocol::parent_type>::type,
            typename Protocol::type
        >::type type;
    };

    template<class Event>
    struct enumerate {
        typedef protocol<typename Event::tag> protocol_type;
        typedef typename flatten<protocol_type>::type hierarchy_type;

        static_assert(
            mpl::contains<hierarchy_type, Event>::value,
            "event has not been registered within its hierarchy"
        );

        typedef typename mpl::distance<
            typename mpl::begin<hierarchy_type>::type,
            typename mpl::find<hierarchy_type, Event>::type
        >::type type;
    };

    #define DEPENDENT_TYPE(name, default)               \
        template<class E, class = void>                 \
        struct name {                                   \
            typedef default type;                       \
        };                                              \
                                                        \
        template<class E>                               \
        struct name<                                    \
            E,                                          \
            typename depend<typename E::name>::type     \
        >                                               \
        {                                               \
            typedef typename E::name type;              \
        };

    DEPENDENT_TYPE(tuple_type,  mpl::list<>)
    DEPENDENT_TYPE(result_type, void)

    #undef DEPENDENT_TYPE
}

template<class Event>
struct event_traits {
    typedef typename detail::tuple_type<Event>::type tuple_type;
    typedef typename detail::result_type<Event>::type result_type;

    enum constants { id = detail::enumerate<Event>::type::value };
};

}}

#endif
