/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_IO_FROZEN_EVENTS_HPP
#define COCAINE_IO_FROZEN_EVENTS_HPP

#include "cocaine/rpc/slot.hpp"

#include <boost/variant/variant.hpp>

namespace cocaine { namespace io {

namespace mpl = boost::mpl;

// Frozen events

template<class Event>
struct frozen {
    typedef Event event_type;
    typedef typename basic_slot<event_type>::tuple_type tuple_type;

    frozen() = default;

    template<class... Args>
    frozen(event_type, Args&&... args):
        tuple(std::forward<Args>(args)...)
    { }

    // NOTE: If the message cannot be sent right away, then the message arguments are placed into a
    // temporary storage until the upstream is attached.
    tuple_type tuple;
};

template<class Event, class... Args>
frozen<Event>
make_frozen(Args&&... args) {
    return frozen<Event>(Event(), std::forward<Args>(args)...);
}

template<class Tag>
struct make_frozen_over {
    typedef typename mpl::transform<
        typename messages<Tag>::type,
        typename mpl::lambda<frozen<mpl::_1>>
    >::type frozen_types;

    typedef typename boost::make_variant_over<frozen_types>::type type;
};

}} // namespace cocaine::io

#endif
