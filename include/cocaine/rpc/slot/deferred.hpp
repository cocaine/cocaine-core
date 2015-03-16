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

#ifndef COCAINE_IO_DEFERRED_SLOT_HPP
#define COCAINE_IO_DEFERRED_SLOT_HPP

#include "cocaine/rpc/slot/function.hpp"

#include "cocaine/rpc/queue.hpp"

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

    explicit
    deferred_slot(callable_type callable):
        parent_type(callable)
    { }

    virtual
    boost::optional<std::shared_ptr<const dispatch_type>>
    operator()(tuple_type&& args, upstream_type&& upstream) {
        try {
            this->call(std::move(args)).attach(std::move(upstream));
        } catch(const asio::system_error& e) {
            upstream.template send<typename protocol::error>(e.code().value(), e.code().message());
        } catch(const std::exception& e) {
            upstream.template send<typename protocol::error>(error::service_error, std::string(e.what()));
        }

        if(is_recursive<Event>::value) {
            return boost::none;
        } else {
            return boost::make_optional<std::shared_ptr<const dispatch_type>>(nullptr);
        }
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

    typedef io::message_queue<io::primitive_tag<type>> queue_type;
    typedef io::primitive<type> protocol;

    template<template<class> class, class, class> friend struct io::deferred_slot;

    deferred():
        outbox(new synchronized<queue_type>())
    { }

    template<class U>
    typename std::enable_if<
        std::is_convertible<typename pristine<U>::type, T>::value,
        deferred&
    >::type
    write(U&& value) {
        outbox->synchronize()->template append<typename protocol::value>(std::forward<U>(value));
        return *this;
    }

    deferred&
    abort(int code, const std::string& reason) {
        outbox->synchronize()->template append<typename protocol::error>(code, reason);
        return *this;
    }

    template<class UpstreamType>
    void
    attach(UpstreamType&& upstream) {
        outbox->synchronize()->attach(std::move(upstream));
    }

private:
    const std::shared_ptr<synchronized<queue_type>> outbox;
};

template<>
struct deferred<void> {
    typedef aux::reconstruct<void>::type type;

    typedef io::message_queue<io::primitive_tag<type>> queue_type;
    typedef io::primitive<type> protocol;

    template<template<class> class, class, class> friend struct io::deferred_slot;

    deferred():
        outbox(new synchronized<queue_type>())
    { }

    deferred&
    abort(int code, const std::string& reason) {
        outbox->synchronize()->append<protocol::error>(code, reason);
        return *this;
    }

    deferred&
    close() {
        outbox->synchronize()->append<protocol::value>();
        return *this;
    }

    template<class UpstreamType>
    void
    attach(UpstreamType&& upstream) {
        outbox->synchronize()->attach(std::move(upstream));
    }

private:
    const std::shared_ptr<synchronized<queue_type>> outbox;
};

} // namespace cocaine

#endif
