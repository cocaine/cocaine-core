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

#include <mutex>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/get.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/variant.hpp>

namespace cocaine {

template<class T> struct deferred;

namespace io {

// Deferred slot

template<class R, class Event>
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
        typedef deferred<upstream_type> expected_type;

        try {
            // This cast is needed to ensure the correct deferred type.
            static_cast<expected_type>(this->call(unpacked)).attach(upstream);
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

struct unassigned { };

template<class T>
struct value_type {
    value_type(const T& value_):
        value(value_)
    { }

    T value;
};

template<>
struct value_type<void>;

struct error_type {
    error_type(int code_, std::string reason_):
        code(code_),
        reason(reason_)
    { }

    int code;
    std::string reason;
};

struct empty_type { };

template<class T>
struct variations {
    typedef boost::variant<unassigned, value_type<T>, error_type, empty_type> type;
};

template<>
struct variations<void> {
    typedef boost::variant<unassigned, error_type, empty_type> type;
};

template<class T, class Impl>
struct enable_write {
    void
    write(const T& value) {
        auto impl = static_cast<Impl*>(this);

        std::lock_guard<std::mutex> guard(impl->mutex);

        if(!boost::get<unassigned>(&impl->result)) return;

        impl->result = value_type<T>(value);
        impl->flush();
    }
};

template<class Impl>
struct enable_write<void, Impl> { };

template<class T>
struct shared_state:
    public enable_write<T, shared_state<T>>
{
    void
    abort(int code, const std::string& reason) {
        std::lock_guard<std::mutex> guard(mutex);

        if(!boost::get<unassigned>(&result)) return;

        result = error_type(code, reason);
        flush();
    }

    void
    close() {
        std::lock_guard<std::mutex> guard(mutex);

        if(!boost::get<unassigned>(&result)) return;

        result = empty_type();
        flush();
    }

    void
    attach(const std::shared_ptr<upstream_t>& upstream_) {
        std::lock_guard<std::mutex> guard(mutex);

        upstream = upstream_;

        if(!boost::get<unassigned>(&result)) {
            flush();
        }
    }

    friend struct enable_write<T, shared_state<T>>;

private:
    void
    flush() {
        if(upstream) {
            boost::apply_visitor(result_visitor_t(upstream), result);
        }
    }

private:
    typename variations<T>::type result;

    struct result_visitor_t:
        public boost::static_visitor<void>
    {
        result_visitor_t(const std::shared_ptr<upstream_t>& upstream_):
            upstream(upstream_)
        { }

        void
        operator()(const unassigned&) const {
            // Empty.
        }

        void
        operator()(const value_type<T>& result) const {
            upstream->send<typename io::streaming<T>::chunk>(result.value);
            upstream->seal<typename io::streaming<T>::choke>();
        }

        void
        operator()(const error_type& error) const {
            upstream->send<typename io::streaming<T>::error>(error.code, error.reason);
            upstream->seal<typename io::streaming<T>::choke>();
        }

        void
        operator()(const empty_type&) const {
            upstream->seal<typename io::streaming<T>::choke>();
        }

    private:
        const std::shared_ptr<upstream_t>& upstream;
    };

    // The upstream might be attached during state method invocation, so it has to be synchronized
    // with a mutex - the atomicicity guarantee of the shared_ptr is not enough.
    std::shared_ptr<upstream_t> upstream;
    std::mutex mutex;
};

}} // namespace io::aux

template<class T>
struct deferred {
    deferred():
        state(new io::aux::shared_state<T>())
    { }

    void
    attach(const std::shared_ptr<upstream_t>& upstream) {
        state->attach(upstream);
    }

    void
    write(const T& value) {
        state->write(value);
    }

    void
    abort(int code, const std::string& reason) {
        state->abort(code, reason);
    }

private:
    const std::shared_ptr<io::aux::shared_state<T>> state;
};

template<>
struct deferred<void> {
    deferred():
        state(new io::aux::shared_state<void>())
    { }

    void
    attach(const std::shared_ptr<upstream_t>& upstream) {
        state->attach(upstream);
    }

    void
    close() {
        state->close();
    }

    void
    abort(int code, const std::string& reason) {
        state->abort(code, reason);
    }

private:
    const std::shared_ptr<io::aux::shared_state<void>> state;
};

} // namespace cocaine

#endif
