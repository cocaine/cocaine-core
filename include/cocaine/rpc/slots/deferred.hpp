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

#include "cocaine/rpc/slots/function.hpp"

#include "cocaine/rpc/queue.hpp"
#include "cocaine/rpc/result_of.hpp"

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

    typedef typename parent_type::dispatch_type dispatch_type;
    typedef typename parent_type::tuple_type tuple_type;
    typedef typename parent_type::upstream_type upstream_type;

    typedef typename parent_type::protocol protocol;

    deferred_slot(callable_type callable):
        parent_type(callable)
    { }

    virtual
    std::shared_ptr<dispatch_type>
    operator()(tuple_type&& args, upstream_type&& upstream) {
        try {
            const T<R> result = this->call(args);

            // Upstream is attached in a critical section, because the internal message queue might
            // be already in use in some other processing thread of the service.
            (*result.queue_impl)->attach(std::move(upstream));
        } catch(const std::system_error& e) {
            upstream.template send<typename protocol::error>(e.code().value(), std::string(e.code().message()));
        } catch(const std::exception& e) {
            upstream.template send<typename protocol::error>(invocation_error, std::string(e.what()));
        }

        // Return an empty protocol dispatch.
        return std::shared_ptr<dispatch_type>();
    }
};

} // namespace io

namespace aux {

// Reconstructs an MPL list from the user-supplied return type.

template<class T, class = void>
struct reconstruct {
    typedef typename boost::mpl::list<T> type;
};

template<class T>
struct reconstruct<T, typename std::enable_if<std::is_same<T, void>::value>::type> {
    typedef typename boost::mpl::list<> type;
};

template<class T>
struct reconstruct<T, typename std::enable_if<boost::mpl::is_sequence<T>::value>::type> {
    typedef T type;
};

template<typename... Args>
struct reconstruct<std::tuple<Args...>> {
    typedef typename itemize<Args...>::type type;
};

} // namespace aux

template<class T>
struct deferred {
    typedef typename aux::reconstruct<T>::type type;

    typedef io::message_queue<io::streaming_tag<type>> queue_type;
    typedef io::streaming<type> protocol;

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
    typedef aux::reconstruct<void>::type type;

    typedef io::message_queue<io::streaming_tag<type>> queue_type;
    typedef io::streaming<type> protocol;

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
