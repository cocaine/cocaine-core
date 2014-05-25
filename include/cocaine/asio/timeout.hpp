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

#ifndef COCAINE_IO_TIMEOUT_HPP
#define COCAINE_IO_TIMEOUT_HPP

#include "cocaine/asio/reactor.hpp"

namespace cocaine { namespace io {

struct timeout_t {
    COCAINE_DECLARE_NONCOPYABLE(timeout_t)

    timeout_t(reactor_t& reactor):
        m_time_watcher(reactor.native())
    {
        m_time_watcher.set<timeout_t, &timeout_t::on_event>(this);
    }

    template<class TimeoutHandler>
    void
    bind(TimeoutHandler timeout_handler) {
        m_handle_timeout = timeout_handler;
    }

    void
    unbind() {
        if(m_time_watcher.is_active()) {
            m_time_watcher.stop();
        }

        m_handle_timeout = nullptr;
    }

    void
    start(float when, float repeat = 0.0f) {
        m_time_watcher.start(when, repeat);
    }

    void
    stop() {
        m_time_watcher.stop();
    }

private:
    void
    on_event(ev::timer&, int) {
        m_handle_timeout();
    }

private:
    // Time watcher.
    ev::timer m_time_watcher;

    // Timeout callback.
    std::function<void()> m_handle_timeout;
};

}} // namespace cocaine::io

#endif
