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

#include "cocaine/rpc/tags.hpp"

#include <boost/mpl/begin.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/distance.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/joint_view.hpp>
#include <boost/mpl/list.hpp>

namespace cocaine { namespace io {

namespace mpl = boost::mpl;

template<class Tag>
struct protocol;

template<>
struct protocol<void> {
    typedef mpl::list<> messages;
};

template<class Tag>
struct extends {
    typedef protocol<Tag> parent_type;
};

namespace aux {

template<class Protocol, class = void>
struct flatten {
    typedef typename Protocol::messages type;
};

template<class Protocol>
struct flatten<Protocol, typename depend<typename Protocol::parent_type>::type> {
    typedef typename mpl::joint_view<
        typename flatten<typename Protocol::parent_type>::type,
        typename Protocol::messages
    >::type type;
};

template<class Event>
struct enumerate {
    typedef protocol<typename Event::tag> protocol_type;
    typedef typename flatten<protocol_type>::type hierarchy_type;

    static_assert(
        mpl::contains<hierarchy_type, Event>::value,
        "message has not been registered within its protocol hierarchy"
    );

    typedef typename mpl::distance<
        typename mpl::begin<hierarchy_type>::type,
        typename mpl::find<hierarchy_type, Event>::type
    >::type type;
};

template<class Event, class = void>
struct transition_type {
    typedef void type;
};

template<class Event>
struct transition_type<Event, typename depend<typename Event::transition_type>::type> {
    typedef typename Event::transition_type type;
};

template<class Event, class = void>
struct tuple_type {
    typedef mpl::list<> type;
};

template<class Event>
struct tuple_type<Event, typename depend<typename Event::tuple_type>::type> {
    typedef typename Event::tuple_type type;
};

template<class Event, class = void>
struct drain_type {
    typedef streaming_tag<void> type;
};

template<class Event>
struct drain_type<Event, typename depend<typename Event::drain_type>::type> {
    typedef typename Event::drain_type type;
};

} // namespace aux

template<class Event>
struct event_traits {
    enum constants { id = aux::enumerate<Event>::type::value };

    // Transition is a protocol tag type of the service channel dispatch after the given message is
    // successfully processed. The possible transitions types are: void, recursive protocol tag or
    // some arbitrary protocol tag.
    // By default, all messages switch the service protocol to the void protocol, which means that
    // no other messages can be sent in the channel until the invocation is complete.
    typedef typename aux::transition_type<Event>::type transition_type;

    // Tuple is the type list of the message arguments.
    // By default, all messages have no arguments, the only information they provide is their type.
    typedef typename aux::tuple_type<Event>::type tuple_type;

    // Drain is a protocol tag type of all the possible messages that a service might send back in
    // response to the given message, i.e. it's a protocol tag type of the client dispatch after the
    // given message is sent.
    // By default, all messages use the void streaming protocol to send back the invocation errors
    // and signal message processing completion.
    typedef typename aux::drain_type<Event>::type drain_type;
};

}} // namespace cocaine::io

#endif
