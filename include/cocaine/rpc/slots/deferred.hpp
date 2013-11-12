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

#include <boost/variant/get.hpp>
#include <boost/variant/variant.hpp>

namespace cocaine { namespace io {

// Deferred slot

template<class R, class Event>
struct deferred_slot:
    public function_slot<R, Event>
{
    typedef function_slot<R, Event> parent_type;
    typedef typename parent_type::callable_type callable_type;

    deferred_slot(callable_type callable):
        parent_type(callable)
    { }

    virtual
    std::shared_ptr<dispatch_t>
    operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
        this->call(unpacked).attach(upstream);

        // Return an empty protocol dispatch.
        return std::shared_ptr<dispatch_t>();
    }
};

namespace detail {

struct shared_state_t {
    template<class T>
    void
    write(const T& value) {
        std::lock_guard<std::mutex> guard(mutex);

        if(!boost::get<unassigned>(&result)) return;

        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> packer(buffer);

        io::type_traits<T>::pack(packer, value);

        value_type serialized;

        serialized.size = buffer.size();
        serialized.blob = buffer.release();

        result = serialized;
        flush();
    }

    void
    abort(int code, const std::string& reason);

    void
    close();

    void
    attach(const api::stream_ptr_t& upstream);

private:
    void
    flush();

private:
    struct unassigned { };
    struct value_type { size_t size; char * blob; };
    struct error_type { int code; std::string reason; };
    struct empty_type { };

    boost::variant<unassigned, value_type, error_type, empty_type> result;

    struct result_visitor_t;

    // The upstream might be attached during state method invocation, so it has to be synchronized
    // with a mutex - the atomicicity guarantee of the shared_ptr is not enough.
    api::stream_ptr_t upstream;
    std::mutex        mutex;
};

}} // namespace io::detail

template<class T>
struct deferred {
    deferred():
        state(new io::detail::shared_state_t())
    { }

    void
    attach(const api::stream_ptr_t& upstream) {
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
    const std::shared_ptr<io::detail::shared_state_t> state;
};

template<>
struct deferred<void> {
    deferred():
        state(new io::detail::shared_state_t())
    { }

    void
    attach(const api::stream_ptr_t& upstream) {
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
    const std::shared_ptr<io::detail::shared_state_t> state;
};

} // namespace cocaine

#endif
