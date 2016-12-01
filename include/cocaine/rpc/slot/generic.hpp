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

#ifndef COCAINE_IO_GENERIC_SLOT_HPP
#define COCAINE_IO_GENERIC_SLOT_HPP

#include "cocaine/rpc/slot.hpp"

#include "cocaine/tuple.hpp"

#include <asio/system_error.hpp>

namespace cocaine { namespace io {

template<class Event>
struct generic_slot:
    public basic_slot<Event>
{
    typedef typename basic_slot<Event>::dispatch_type dispatch_type;
    typedef typename basic_slot<Event>::meta_type meta_type;
    typedef typename basic_slot<Event>::tuple_type    tuple_type;
    typedef typename basic_slot<Event>::upstream_type upstream_type;
    typedef typename basic_slot<Event>::result_type result_type;
    typedef std::function<result_type(const meta_type& meta, tuple_type&& args, upstream_type&& upstream)> function_type;

    explicit
    generic_slot(function_type callable_):
        callable(std::move(callable_))
    {}


    result_type
    operator()(const meta_type& meta, tuple_type&& args, upstream_type&& upstream) override {
        return callable(meta, std::forward<tuple_type>(args), std::forward<upstream_type>(upstream));
    }

private:
    function_type callable;
};

}} // namespace cocaine::io

#endif
