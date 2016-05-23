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
    class ForwardHeaders,
    class R = typename result_of<Event>::type
>
struct deferred_slot:
    public function_slot<Event, T<R>, ForwardHeaders>
{
    typedef function_slot<Event, T<R>, ForwardHeaders> parent_type;

    typedef typename parent_type::callable_type callable_type;
    typedef typename parent_type::dispatch_type dispatch_type;
    typedef typename parent_type::tuple_type    tuple_type;
    typedef typename parent_type::upstream_type upstream_type;
    typedef typename parent_type::protocol      protocol;

    explicit
    deferred_slot(callable_type callable):
        parent_type(callable)
    { }

    virtual
    boost::optional<std::shared_ptr<const dispatch_type>>
    operator()(tuple_type&& args,
               upstream_type&& upstream)
    {
        return operator()({}, std::move(args), std::move(upstream));
    }

    virtual
    boost::optional<std::shared_ptr<const dispatch_type>>
    operator()(const std::vector<hpack::header_t>& headers,
               tuple_type&& args,
               upstream_type&& upstream)
    {
        try {
            this->call(headers, std::move(args)).attach(std::move(upstream));
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

template<class... Args>
struct reconstruct<std::tuple<Args...>> {
    typedef typename itemize<Args...>::type type;
};

} // namespace aux

template<class T>
struct deferred {
    typedef typename aux::reconstruct<T>::type type;

    typedef io::message_queue<io::primitive_tag<type>> queue_type;
    typedef io::primitive<type> protocol;

    template<template<class> class, class, class, class> friend struct io::deferred_slot;

    deferred():
        outbox(new synchronized<queue_type>())
    { }

    template<class... Args>
    typename std::enable_if<
        std::is_constructible<T, Args...>::value,
        deferred&
    >::type
    write(Args&&... args) {
        outbox->synchronize()->template append<typename protocol::value>(std::forward<Args>(args)...);
        return *this;
    }

    template<class... Args>
    typename std::enable_if<
        std::is_constructible<T, Args...>::value,
        deferred&
    >::type
    write(hpack::header_storage_t headers, Args&&... args) {
        outbox->synchronize()->template append<typename protocol::value>(std::move(headers), std::forward<Args>(args)...);
        return *this;
    }

    deferred&
    abort(const std::error_code& ec, const std::string& reason) {
        outbox->synchronize()->template append<typename protocol::error>(ec, reason);
        return *this;
    }

    deferred&
    abort(hpack::header_storage_t headers, const std::error_code& ec, const std::string& reason) {
        outbox->synchronize()->template append<typename protocol::error>(std::move(headers), ec, reason);
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

    template<template<class> class, class, class, class> friend struct io::deferred_slot;

    deferred():
        outbox(new synchronized<queue_type>())
    { }

    deferred&
    abort(const std::error_code& ec, const std::string& reason) {
        outbox->synchronize()->append<protocol::error>(ec, reason);
        return *this;
    }

    deferred&
    abort(hpack::header_storage_t headers, const std::error_code& ec, const std::string& reason) {
        outbox->synchronize()->append<protocol::error>(std::move(headers), ec, reason);
        return *this;
    }


    deferred&
    close(hpack::header_storage_t headers) {
        outbox->synchronize()->append<protocol::value>(std::move(headers));
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
