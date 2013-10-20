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

} // namespace io

namespace detail {
    struct state_t {
        state_t();

        template<class T>
        void
        write(const T& value) {
            std::lock_guard<std::mutex> guard(m_mutex);

            if(m_completed) {
                return;
            }

            io::type_traits<T>::pack(m_packer, value);

            if(m_upstream) {
                m_upstream->write(m_buffer.data(), m_buffer.size());
                m_upstream->close();
            }

            m_completed = true;
        }

        void
        abort(int code, const std::string& reason);

        void
        close();

        void
        attach(const api::stream_ptr_t& upstream);

    private:
        msgpack::sbuffer m_buffer;
        msgpack::packer<msgpack::sbuffer> m_packer;

        int m_code;
        std::string m_reason;

        bool m_completed,
             m_failed;

        api::stream_ptr_t m_upstream;
        std::mutex m_mutex;
    };
}

template<class T>
struct deferred {
    deferred():
        m_state(new detail::state_t())
    { }

    void
    attach(const api::stream_ptr_t& upstream) {
        m_state->attach(upstream);
    }

    void
    write(const T& value) {
        m_state->write(value);
    }

    void
    abort(int code, const std::string& reason) {
        m_state->abort(code, reason);
    }

private:
    const std::shared_ptr<detail::state_t> m_state;
};

template<>
struct deferred<void> {
    deferred():
        m_state(new detail::state_t())
    { }

    void
    attach(const api::stream_ptr_t& upstream) {
        m_state->attach(upstream);
    }

    void
    close() {
        m_state->close();
    }

    void
    abort(int code, const std::string& reason) {
        m_state->abort(code, reason);
    }

private:
    const std::shared_ptr<detail::state_t> m_state;
};

} // namespace cocaine

#endif
