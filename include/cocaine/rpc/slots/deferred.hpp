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

#ifndef COCAINE_IO_DEFERRED_SLOT_HPP
#define COCAINE_IO_DEFERRED_SLOT_HPP

#include "cocaine/rpc/queue.hpp"
#include "cocaine/rpc/result_of.hpp"

#include "cocaine/rpc/slots/function.hpp"

namespace cocaine { namespace io {

template<
    template<class> class T,
    class Event,
    class R = typename result_of<Event>::type
>
struct deferred_slot:
    public function_slot<Event, T<R>>
{
    typedef function_slot<Event, T<R>> parent_type;

    typedef typename parent_type::callable_type callable_type;
    typedef typename parent_type::protocol_type protocol;

    deferred_slot(callable_type callable):
        parent_type(callable)
    { }

    typedef typename parent_type::tuple_type tuple_type;

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const tuple_type& args, const std::shared_ptr<upstream_t>& upstream) {
        try {
            const T<R> result = this->call(args);

            // Upstream is attached in a critical section, because the internal message queue might
            // be already in use in some other processing thread of the service.
            (*result.queue_impl)->attach(upstream);
        } catch(const std::system_error& e) {
            upstream->send<typename protocol::error>(e.code().value(), std::string(e.code().message()));
        } catch(const std::exception& e) {
            upstream->send<typename protocol::error>(invocation_error, std::string(e.what()));
        }

        // Return an empty protocol dispatch.
        return std::shared_ptr<dispatch_t>();
    }
};

} // namespace io

template<class T>
struct deferred {
    typedef io::message_queue<io::streaming_tag<T>> queue_type;
    typedef io::streaming<T> protocol;

    template<template<class> class, class, class> friend struct io::deferred_slot;

    deferred():
        queue_impl(std::make_shared<synchronized<queue_type>>())
    { }

    template<class U>
    void
    write(U&& value,
          typename std::enable_if<std::is_convertible<typename pristine<U>::type, T>::value>::type* = nullptr)
    {
        auto locked = (*queue_impl).synchronize();

        locked->template append<typename protocol::chunk>(std::forward<U>(value));
        locked->template append<typename protocol::choke>();
    }

    void
    abort(int code, const std::string& reason) {
        (*queue_impl)->template append<typename protocol::error>(code, reason);
    }

private:
    const std::shared_ptr<synchronized<queue_type>> queue_impl;
};

template<>
struct deferred<void> {
    typedef io::message_queue<io::streaming_tag<void>> queue_type;
    typedef io::streaming<void> protocol;

    template<template<class> class, class, class> friend struct io::deferred_slot;

    deferred():
        queue_impl(std::make_shared<synchronized<queue_type>>())
    { }

    void
    abort(int code, const std::string& reason) {
        (*queue_impl)->append<protocol::error>(code, reason);
    }

    void
    close() {
        (*queue_impl)->append<protocol::choke>();
    }

private:
    const std::shared_ptr<synchronized<queue_type>> queue_impl;
};

} // namespace cocaine

#endif
