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

#include "cocaine/rpc/slots/function.hpp"

namespace cocaine { namespace io {

// Deferred slot

template<template<class> class T, class R, class Event>
struct deferred_slot:
    public function_slot<R, Event>
{
    typedef function_slot<R, Event> parent_type;

    typedef typename parent_type::callable_type callable_type;
    typedef typename parent_type::upstream_type upstream_type;

    typedef io::streaming<upstream_type> protocol;

    deferred_slot(callable_type callable):
        parent_type(callable)
    { }

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const msgpack::object& unpacked, const std::shared_ptr<upstream_t>& upstream) {
        typedef T<typename result_of<Event>::type> expected_type;

        try {
            // This cast is needed to ensure the correct return type.
            auto object = static_cast<expected_type>(this->call(unpacked));

            {
                std::lock_guard<typename expected_type::queue_type> guard(*object.queue_impl);

                // Upstream is attached in a critical section, because it might be already in use
                // in some other processing thread of the service.
                object.queue_impl->attach(upstream);
            }
        } catch(const std::system_error& e) {
            upstream->send<typename protocol::error>(e.code().value(), std::string(e.code().message()));
            upstream->seal<typename protocol::choke>();
        } catch(const std::exception& e) {
            upstream->send<typename protocol::error>(invocation_error, std::string(e.what()));
            upstream->seal<typename protocol::choke>();
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
        queue_impl(std::make_shared<queue_type>())
    { }

    template<class U>
    void
    write(U&& value, typename std::enable_if<std::is_convertible<U, T>::value>::type* = nullptr) {
        std::lock_guard<queue_type> guard(*queue_impl);
        queue_impl->template append<typename protocol::chunk>(false, std::forward<U>(value));
        queue_impl->template append<typename protocol::choke>(true);
    }

    void
    abort(int code, const std::string& reason) {
        std::lock_guard<queue_type> guard(*queue_impl);
        queue_impl->template append<typename protocol::error>(false, code, reason);
        queue_impl->template append<typename protocol::choke>(true);
    }

private:
    const std::shared_ptr<queue_type> queue_impl;
};

template<>
struct deferred<void> {
    typedef io::message_queue<io::streaming_tag<void>> queue_type;
    typedef io::streaming<void> protocol;

    template<template<class> class, class, class> friend struct io::deferred_slot;

    deferred():
        queue_impl(std::make_shared<queue_type>())
    { }

    void
    abort(int code, const std::string& reason) {
        std::lock_guard<queue_type> guard(*queue_impl);
        queue_impl->template append<typename protocol::error>(false, code, reason);
        queue_impl->template append<typename protocol::choke>(true);
    }

    void
    close() {
        std::lock_guard<queue_type> guard(*queue_impl);
        queue_impl->template append<typename protocol::choke>(true);
    }

private:
    const std::shared_ptr<queue_type> queue_impl;
};

} // namespace cocaine

#endif
