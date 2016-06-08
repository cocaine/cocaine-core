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

#ifndef COCAINE_IO_BLOCKING_SLOT_HPP
#define COCAINE_IO_BLOCKING_SLOT_HPP

#include "cocaine/rpc/slot/function.hpp"

#include <boost/optional/optional.hpp>

namespace cocaine { namespace io {

template<
    class Event,
    class ForwardHeaders,
    class R = typename result_of<Event>::type
>
struct blocking_slot:
    public function_slot<Event, typename result_of<Event>::type, ForwardHeaders>
{
    typedef function_slot<Event, R, ForwardHeaders> parent_type;

    typedef typename parent_type::callable_type callable_type;
    typedef typename parent_type::dispatch_type dispatch_type;
    typedef typename parent_type::tuple_type    tuple_type;
    typedef typename parent_type::upstream_type upstream_type;
    typedef typename parent_type::protocol      protocol;

    explicit
    blocking_slot(callable_type callable):
        parent_type(callable)
    { }

    virtual
    boost::optional<std::shared_ptr<const dispatch_type>>
    operator()(const std::vector<hpack::header_t>& headers,
               tuple_type&& args,
               upstream_type&& upstream)
    {
        try {
            upstream.template send<typename protocol::value>(this->call(headers, std::move(args)));
        } catch(const std::system_error& e) {
            upstream.template send<typename protocol::error>(e.code(), std::string(e.what()));
        } catch(const std::exception& e) {
            upstream.template send<typename protocol::error>(error::uncaught_error, std::string(e.what()));
        }

        if(is_recursed<Event>::value) {
            return boost::none;
        } else {
            return boost::make_optional<std::shared_ptr<const dispatch_type>>(nullptr);
        }
    }
};

// Blocking slot specialization for void functions (returning completion status)

template<class Event, class ForwardHeaders>
struct blocking_slot<Event, ForwardHeaders, void>:
    public function_slot<Event, void, ForwardHeaders>
{
    typedef function_slot<Event, void, ForwardHeaders> parent_type;

    typedef typename parent_type::callable_type callable_type;
    typedef typename parent_type::dispatch_type dispatch_type;
    typedef typename parent_type::tuple_type    tuple_type;
    typedef typename parent_type::upstream_type upstream_type;
    typedef typename parent_type::protocol      protocol;

    explicit
    blocking_slot(callable_type callable):
        parent_type(callable)
    { }

    virtual
    boost::optional<std::shared_ptr<const dispatch_type>>
    operator()(const std::vector<hpack::header_t>& headers,
               tuple_type&& args,
               upstream_type&& upstream)
    {
        try {
            this->call(headers, std::move(args));

            // This is needed anyway so that service clients could detect operation completion.
            upstream.template send<typename protocol::value>();
        } catch(const std::system_error& e) {
            upstream.template send<typename protocol::error>(e.code(), std::string(e.what()));
        } catch(const std::exception& e) {
            upstream.template send<typename protocol::error>(error::uncaught_error, std::string(e.what()));
        }

        if(is_recursed<Event>::value) {
            return boost::none;
        } else {
            return boost::make_optional<std::shared_ptr<const dispatch_type>>(nullptr);
        }
    }
};

// Blocking slot specialization for functions, that returns only additional headers.

// template<class Event, class ForwardHeaders>
// struct blocking_slot<Event, ForwardHeaders, std::vector<hpack::header_t>>:

// Blocking slot specialization for functions, that returns additional headers as like as args.

// template<class Event, class ForwardHeaders, class R>
// struct blocking_slot<Event, ForwardHeaders, std::tuple<headers_t, R>>;


// Blocking slot specialization for mute functions (returning nothing at all)

template<class Event, class ForwardHeaders>
struct blocking_slot<Event, ForwardHeaders, mute_slot_tag>:
    public function_slot<Event, void, ForwardHeaders>
{
    typedef function_slot<Event, void, ForwardHeaders> parent_type;

    typedef typename parent_type::callable_type callable_type;
    typedef typename parent_type::dispatch_type dispatch_type;
    typedef typename parent_type::tuple_type    tuple_type;
    typedef typename parent_type::upstream_type upstream_type;

    explicit
    blocking_slot(callable_type callable):
        parent_type(callable)
    { }

    virtual
    boost::optional<std::shared_ptr<const dispatch_type>>
    operator()(const std::vector<hpack::header_t>& headers,
               tuple_type&& args,
               upstream_type&&)
    {
        // NOTE: Since the slot is mute, i.e. there's no upstream it can use to send the error info
        // back to the client, we just pass on exceptions here and let dispatch handle them.
        this->call(headers, std::move(args));

        if(is_recursed<Event>::value) {
            return boost::none;
        } else {
            return boost::make_optional<std::shared_ptr<const dispatch_type>>(nullptr);
        }
    }
};

}} // namespace cocaine::io

#endif
