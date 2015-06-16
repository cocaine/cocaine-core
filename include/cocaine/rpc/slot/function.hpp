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

#ifndef COCAINE_IO_FUNCTION_SLOT_HPP
#define COCAINE_IO_FUNCTION_SLOT_HPP

#include "cocaine/idl/primitive.hpp"
#include "cocaine/idl/streaming.hpp"

#include "cocaine/rpc/result_of.hpp"
#include "cocaine/rpc/slot.hpp"

#include "cocaine/traits/enum.hpp"
#include "cocaine/traits/error_code.hpp"

#include "cocaine/tuple.hpp"

#include <boost/function_types/function_type.hpp>
#include <boost/mpl/push_front.hpp>

#include <asio/system_error.hpp>

namespace cocaine { namespace io {

template<class Event, class R> struct function_slot;

namespace aux {

// Common slots (blocking, deferred, streamed) work only for streaming and void protocols, so other
// protocols are statically disabled here.

template<class Tag>
struct protocol_impl;

template<class T>
struct protocol_impl<primitive_tag<T>> {
    typedef typename protocol<primitive_tag<T>>::scope type;
};

template<class T>
struct protocol_impl<streaming_tag<T>> {
    typedef typename protocol<streaming_tag<T>>::scope type;
};

template<>
struct protocol_impl<void> {
    // Undefined scope.
    typedef struct { } type;
};

} // namespace aux

namespace mpl = boost::mpl;
namespace bft = boost::function_types;

template<class Event, class R>
struct function_slot:
    public basic_slot<Event>
{
    static_assert(
        is_terminal<Event>::value || is_recursed<Event>::value,
        "messages with dispatch transitions are not supported"
    );

    typedef typename bft::function_type<typename mpl::push_front<
        typename basic_slot<Event>::sequence_type,
        R
    >::type>::type function_type;

    typedef std::function<function_type> callable_type;

    typedef typename basic_slot<Event>::dispatch_type dispatch_type;
    typedef typename basic_slot<Event>::tuple_type    tuple_type;
    typedef typename basic_slot<Event>::upstream_type upstream_type;

    typedef typename aux::protocol_impl<typename event_traits<
        Event
    >::upstream_type>::type protocol;

    explicit
    function_slot(callable_type callable_):
        callable(callable_)
    { }

    R
    call(tuple_type&& args) const {
        return tuple::invoke(std::move(args), callable);
    }

private:
    const callable_type callable;
};

}} // namespace cocaine::io

#endif
