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

#ifndef COCAINE_IO_SLOT_HPP
#define COCAINE_IO_SLOT_HPP

#include "cocaine/rpc/protocol.hpp"
#include "cocaine/rpc/upstream.hpp"

#include "cocaine/tuple.hpp"

#include <boost/mpl/lambda.hpp>
#include <boost/mpl/transform.hpp>

namespace cocaine { namespace io {

namespace mpl = boost::mpl;

template<class Event>
class basic_slot {
    typedef Event event_type;
    typedef event_traits<event_type> traits_type;

public:
    typedef typename mpl::transform<
        typename traits_type::tuple_type,
        typename mpl::lambda<io::detail::unwrap_type<mpl::_1>>
    >::type sequence_type;

    virtual
   ~basic_slot() {
       // Empty.
    }

    // Expected transition and upstream dispatch types.
    typedef dispatch<typename traits_type::dispatch_type> dispatch_type;
    typedef upstream<typename traits_type::upstream_type> upstream_type;

    // Expected parameter types, stripped of any tags.
    typedef typename tuple::fold<sequence_type>::type tuple_type;

    virtual
    std::shared_ptr<dispatch_type>
    operator()(tuple_type&& args, upstream_type&& upstream) = 0;

public:
    std::string
    name() const {
        return event_type::alias();
    }
};

}} // namespace cocaine::io

#endif
