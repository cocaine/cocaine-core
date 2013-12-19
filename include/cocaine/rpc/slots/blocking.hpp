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

#ifndef COCAINE_IO_BLOCKING_SLOT_HPP
#define COCAINE_IO_BLOCKING_SLOT_HPP

#include "cocaine/rpc/slots/function.hpp"

namespace cocaine { namespace io {

template<class Event, bool Void>
struct result_type_impl {
     typedef typename result_of<Event>::type type;
};

template<class Event>
struct result_type_impl<Event, true> {
    typedef void type;
};

template<
    class Event,
    bool Void = std::is_same<typename io::event_traits<Event>::drain_type, void>::value,
    class R = typename result_type_impl<Event, Void>::type
>
struct blocking_slot:
    public function_slot<Event, R>
{
    typedef function_slot<Event, R> parent_type;

    typedef typename parent_type::callable_type callable_type;
    typedef typename parent_type::protocol_type protocol;

    blocking_slot(callable_type callable):
        parent_type(callable)
    { }

    typedef typename parent_type::tuple_type tuple_type;

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const tuple_type& args, const std::shared_ptr<upstream_t>& upstream) {
        try {
            upstream->send<typename protocol::chunk>(this->call(args));
            upstream->send<typename protocol::choke>();
        } catch(const std::system_error& e) {
            upstream->send<typename protocol::error>(e.code().value(), std::string(e.code().message()));
        } catch(const std::exception& e) {
            upstream->send<typename protocol::error>(invocation_error, std::string(e.what()));
        }

        // Return an empty protocol dispatch.
        return std::shared_ptr<dispatch_t>();
    }
};

// Blocking slot specialization for void functions

template<class Event>
struct blocking_slot<Event, false, void>:
    public function_slot<Event, void>
{
    typedef function_slot<Event, void> parent_type;

    typedef typename parent_type::callable_type callable_type;
    typedef typename parent_type::protocol_type protocol;

    blocking_slot(callable_type callable):
        parent_type(callable)
    { }

    typedef typename parent_type::tuple_type tuple_type;

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const tuple_type& args, const std::shared_ptr<upstream_t>& upstream) {
        try {
            this->call(args);

            // This is needed anyway so that service clients could detect operation completion.
            upstream->send<typename protocol::choke>();
        } catch(const std::system_error& e) {
            upstream->send<typename protocol::error>(e.code().value(), std::string(e.code().message()));
        } catch(const std::exception& e) {
            upstream->send<typename protocol::error>(invocation_error, std::string(e.what()));
        }

        // Return an empty protocol dispatch.
        return std::shared_ptr<dispatch_t>();
    }
};

template<class Event>
struct blocking_slot<Event, true, void>:
    public function_slot<Event, void>
{
    typedef function_slot<Event, void> parent_type;

    typedef typename parent_type::callable_type callable_type;
    typedef typename parent_type::protocol_type protocol;

    blocking_slot(callable_type callable):
        parent_type(callable)
    { }

    typedef typename parent_type::tuple_type tuple_type;

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const tuple_type& args, const std::shared_ptr<upstream_t>& upstream) {
        try {
            this->call(args);
        } catch(const std::exception& e) {
            throw cocaine::error_t("error while calling a terminal slot - %s", e.what());
        }

        // Return an empty protocol dispatch.
        return std::shared_ptr<dispatch_t>();
    }
};

}} // namespace cocaine::io

#endif
