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

namespace cocaine {

template<class T> struct streamed;

namespace io {

// Deferred slot

template<class R, class Event>
struct streamed_slot:
    public function_slot<R, Event>
{
    typedef function_slot<R, Event> parent_type;

    typedef typename parent_type::callable_type callable_type;
    typedef typename parent_type::upstream_type upstream_type;

    typedef io::streaming<upstream_type> protocol;

    streamed_slot(callable_type callable):
        parent_type(callable)
    { }

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const msgpack::object& unpacked, const std::shared_ptr<upstream_t>& upstream) {
        typedef streamed<typename result_of<Event>::type> expected_type;

        try {
            // This cast is needed to ensure the correct streamed type.
            static_cast<expected_type>(this->call(unpacked)).state_impl->attach(upstream);
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

namespace aux {

template<class Event>
struct frozen {
    // NOTE: If the message cannot be sent right away, simply move the message arguments to a
    // temporary storage and wait for the upstream to be attached.
    const typename tuple::fold<typename event_traits<Event>::tuple_type>::type tuple;
};

template<class T>
struct stream_state {
    typedef io::streaming<T> protocol;

    template<class U>
    void
    write(U&& value) {
        std::lock_guard<std::mutex> guard(mutex);

        operations.emplace_back(frozen<typename protocol::chunk>{std::forward<U>(value)});

        if(upstream) play();
    }

    void
    abort(int code, const std::string& reason) {
        std::lock_guard<std::mutex> guard(mutex);

        operations.emplace_back(frozen<typename protocol::error>{code, reason});
        operations.emplace_back(frozen<typename protocol::choke>{});

        if(upstream) play();
    }

    void
    close() {
        std::lock_guard<std::mutex> guard(mutex);

        operations.emplace_back(frozen<typename protocol::choke>{});

        if(upstream) play();
    }

    void
    attach(const std::shared_ptr<upstream_t>& upstream_) {
        std::lock_guard<std::mutex> guard(mutex);

        upstream = upstream_;

        if(!operations.empty()) play();
    }

private:
    void
    play() {
        result_visitor_t visitor(upstream);

        for(auto it = operations.begin(); it != operations.end(); ++it) {
            boost::apply_visitor(visitor, *it);
        }

        operations.clear();
    }

private:
    typedef boost::variant<
        frozen<typename protocol::chunk>,
        frozen<typename protocol::error>,
        frozen<typename protocol::choke>
    > element_type;

    // Operation log.
    std::vector<element_type> operations;

    struct result_visitor_t:
        public boost::static_visitor<void>
    {
        result_visitor_t(const std::shared_ptr<upstream_t>& upstream_):
            upstream(upstream_)
        { }

        void
        operator()(const frozen<typename protocol::chunk>& chunk) const {
            upstream->send<typename protocol::chunk>(std::get<0>(chunk.tuple));
        }

        void
        operator()(const frozen<typename protocol::error>& error) const {
            upstream->send<typename protocol::error>(std::get<0>(error.tuple), std::get<1>(error.tuple));
        }

        void
        operator()(const frozen<typename protocol::choke>&) const {
            upstream->seal<typename protocol::choke>();
        }

    private:
        const std::shared_ptr<upstream_t>& upstream;
    };

    // The upstream might be attached during state method invocation, so it has to be synchronized
    // with a mutex - the atomicicity guarantee of the shared_ptr<T> is not enough.
    std::shared_ptr<upstream_t> upstream;
    std::mutex mutex;
};

}} // namespace io::aux

template<class T>
struct streamed {
    typedef io::aux::stream_state<T> state_type;

    template<class R, class Event>
        friend struct io::streamed_slot;

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
