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

#ifndef COCAINE_IO_PROTOCOL_HPP
#define COCAINE_IO_PROTOCOL_HPP

#include "cocaine/rpc/sanitize.hpp"
#include "cocaine/tuple.hpp"

#include <boost/mpl/begin.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/distance.hpp>
#include <boost/mpl/end.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/insert_range.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/transform.hpp>

#include <cstdint>

namespace cocaine { namespace io {

namespace mpl = boost::mpl;

template<class Tag>
struct protocol;

template<>
struct protocol<void> {
    // Traversal termination.
    typedef mpl::list<> messages;
};

template<class Tag>
struct extends {
    typedef protocol<Tag> parent_type;
};

namespace aux {

template<class Protocol, class = void>
struct flatten_impl {
    typedef typename Protocol::messages type;
};

template<class Protocol>
struct flatten_impl<Protocol, typename depend<typename Protocol::parent_type>::type> {
    // Recursively merge all message typelists from the protocol ancestry.
    typedef typename flatten_impl<typename Protocol::parent_type>::type ancestry_type;

    typedef typename mpl::insert_range<
        ancestry_type,
        typename mpl::end<ancestry_type>::type,
        typename Protocol::messages
    >::type type;
};

} // namespace aux

template<class Tag>
struct messages {
    typedef typename aux::flatten_impl<protocol<Tag>>::type type;
};

namespace aux {

template<class Event>
struct enumerate {
    typedef typename messages<typename Event::tag>::type hierarchy_type;

    static_assert(
        mpl::contains<hierarchy_type, Event>::value,
        "message has not been registered within its protocol hierarchy"
    );

    static const uint64_t value = mpl::distance<
        typename mpl::begin<hierarchy_type>::type,
        typename mpl::find <hierarchy_type, Event>::type
    >::value;
};

template<class Event, class = void>
struct argument_type {
    typedef mpl::list<> type;
};

template<class Event>
struct argument_type<Event, typename depend<typename Event::argument_type>::type> {
    typedef typename Event::argument_type type;
};

template<class Event, class = void>
struct dispatch_type {
    typedef void type;
};

template<class Event>
struct dispatch_type<Event, typename depend<typename Event::dispatch_type>::type> {
    typedef typename Event::dispatch_type type;
};

template<class Event, class = void>
struct upstream_type {
    typedef primitive_tag<mpl::list<>> type;
};

template<class Event>
struct upstream_type<Event, typename depend<typename Event::upstream_type>::type> {
    typedef typename Event::upstream_type type;
};

} // namespace aux

template<class Event>
struct event_traits {
    enum constants { id = aux::enumerate<Event>::value };

    // Tuple is the type list of the message arguments.
    // By default, all messages have no arguments, the only information they provide is their type.
    typedef typename aux::argument_type<Event>::type argument_type;

    /// Sequence type is an unwrapped argument type.
    ///
    /// For example:
    ///     boost::mpl::list<>                      -> boost::mpl::list<>.
    ///     boost::mpl::list<int, string>           -> boost::mpl::list<int, string>.
    ///     boost::mpl::list<int, optional<string>> -> boost::mpl::list<int, string>.
    typedef typename boost::mpl::transform<
        argument_type,
        typename boost::mpl::lambda<io::details::unwrap_type<boost::mpl::_1>>::type
    >::type sequence_type;

    /// Packed tuple type of event handler arguments.
    ///
    /// All internal tagged wrappers (i.e. optional) are unwrapped during construction of this type.
    ///
    /// For example:
    ///     boost::mpl::list<>                      -> std::tuple<>.
    ///     boost::mpl::list<int, string>           -> std::tuple<int, string>.
    ///     boost::mpl::list<int, optional<string>> -> std::tuple<int, string>.
    typedef typename tuple::fold<sequence_type>::type tuple_type;

    static_assert(sanitize<argument_type>::value,
        "mixing optional and non-optional message arguments is not allowed");

    // Dispatch is a protocol tag type of the service channel dispatch after the given message is
    // successfully processed. The possible transitions types are: void, recursive protocol tag or
    // some arbitrary protocol tag.
    // By default, all messages switch the service protocol to the void protocol, which means that
    // no other messages can be sent in the channel until the invocation is complete.
    typedef typename aux::dispatch_type<Event>::type dispatch_type;

    // Upstream is a protocol tag type of all the possible messages that a service might send back
    // in response to the given message, i.e. it's a protocol tag type of the client dispatch after
    // the given message is sent.
    // By default, all messages use the void primitive protocol to send back the invocation errors
    // and signal message processing completion.
    typedef typename aux::upstream_type<Event>::type upstream_type;
};

}} // namespace cocaine::io

#endif
