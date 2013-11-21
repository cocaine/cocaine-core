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

#ifndef COCAINE_IO_STREAMED_SLOT_HPP
#define COCAINE_IO_STREAMED_SLOT_HPP

#include "cocaine/rpc/slots/deferred.hpp"

namespace cocaine { namespace io { namespace aux {

template<class Event>
struct frozen {
    template<typename... Args>
    frozen(bool final_, Args&&... args):
        final(final_),
        tuple(std::forward<Args>(args)...)
    { }

    // NOTE: Indicates that this event should seal the upstream.
    bool final;

    // NOTE: If the message cannot be sent right away, simply move the message arguments to a
    // temporary storage and wait for the upstream to be attached.
    typename tuple::fold<typename event_traits<Event>::tuple_type>::type tuple;
};

struct frozen_visitor_t:
    public boost::static_visitor<void>
{
    frozen_visitor_t(const std::shared_ptr<upstream_t>& upstream_):
        upstream(upstream_)
    { }

    template<class Event>
    void
    operator()(const frozen<Event>& event) const {
        if(!event.final) {
            upstream->send<Event>(event.tuple);
        } else {
            upstream->seal<Event>(event.tuple);
        }
    }

private:
    const std::shared_ptr<upstream_t>& upstream;
};

template<class T>
class stream_state {
    typedef io::streaming<T> protocol;

    typedef boost::variant<
        frozen<typename protocol::chunk>,
        frozen<typename protocol::error>,
        frozen<typename protocol::choke>
    > element_type;

    // Operation log.
    std::vector<element_type> operations;

    // The upstream might be attached during state method invocation, so it has to be synchronized
    // with a mutex - the atomicicity guarantee of the shared_ptr<T> is not enough.
    std::shared_ptr<upstream_t> upstream;
    std::mutex mutex;

private:
    void
    flush() {
        frozen_visitor_t visitor(upstream);

        for(auto it = operations.begin(); it != operations.end(); ++it) {
            boost::apply_visitor(visitor, *it);
        }

        operations.clear();
    }

public:
    template<class U>
    void
    write(U&& value) {
        std::lock_guard<std::mutex> guard(mutex);

        operations.emplace_back(frozen<typename protocol::chunk>(false, std::forward<U>(value)));

        if(upstream) flush();
    }

    void
    abort(int code, const std::string& reason) {
        std::lock_guard<std::mutex> guard(mutex);

        operations.emplace_back(frozen<typename protocol::error>(false, code, reason));
        operations.emplace_back(frozen<typename protocol::choke>(true));

        if(upstream) flush();
    }

    void
    close() {
        std::lock_guard<std::mutex> guard(mutex);

        operations.emplace_back(frozen<typename protocol::choke>(true));

        if(upstream) flush();
    }

    void
    attach(const std::shared_ptr<upstream_t>& upstream_) {
        std::lock_guard<std::mutex> guard(mutex);

        upstream = upstream_;

        if(!operations.empty()) flush();
    }
};

}} // namespace io::aux

template<class T>
struct streamed {
    typedef io::aux::stream_state<T> state_type;

    template<template<class> class, class, class>
        friend struct io::deferred_slot;

    streamed():
        state_impl(std::make_shared<state_type>())
    { }

    template<class U>
    void
    write(U&& value, typename std::enable_if<std::is_convertible<U, T>::value>::type* = nullptr) {
        state_impl->write(std::forward<U>(value));
    }

    void
    abort(int code, const std::string& reason) {
        state_impl->abort(code, reason);
    }

    void
    close() {
        state_impl->close();
    }

private:
    const std::shared_ptr<state_type> state_impl;
};

} // namespace cocaine

#endif
