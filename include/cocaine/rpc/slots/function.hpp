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

#ifndef COCAINE_IO_FUNCTION_SLOT_HPP
#define COCAINE_IO_FUNCTION_SLOT_HPP

#include "cocaine/idl/streaming.hpp"

#include "cocaine/rpc/protocol.hpp"
#include "cocaine/rpc/slot.hpp"

#include "cocaine/traits/enum.hpp"

#include "cocaine/tuple.hpp"

#include <boost/function_types/function_type.hpp>

#include <boost/mpl/lambda.hpp>
#include <boost/mpl/push_front.hpp>
#include <boost/mpl/transform.hpp>

namespace cocaine { namespace io {

template<class Event, class R> struct function_slot;

namespace aux {

template<class Tag>
struct upstream_impl;

template<class T>
struct upstream_impl<streaming_tag<T>> {
    typedef streaming<T> type;
};

} // namespace aux

namespace mpl = boost::mpl;

template<class Event, class R>
struct function_slot:
    public basic_slot<Event>
{
    typedef typename aux::upstream_impl<
        typename event_traits<Event>::drain_type
    >::type protocol_type;

    typedef typename mpl::transform<
        typename event_traits<Event>::tuple_type,
        typename mpl::lambda<detail::unwrap_type<mpl::arg<1>>>::type
    >::type sequence_type;

    typedef typename boost::function_types::function_type<
        typename mpl::push_front<sequence_type, R>::type
    >::type function_type;

    typedef std::function<function_type> callable_type;

    function_slot(callable_type callable_):
        callable(callable_)
    { }

    typedef typename basic_slot<Event>::tuple_type tuple_type;

    R
    call(const tuple_type& args) const {
        return tuple::invoke(callable, args);
    }

private:
    const callable_type callable;
};

}} // namespace cocaine::io

#endif
