/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_ASIO_SERVICE_HPP
#define COCAINE_ASIO_SERVICE_HPP

#include "cocaine/common.hpp"

#include <mutex>

#define EV_MINIMAL       0
#define EV_USE_MONOTONIC 1
#define EV_USE_REALTIME  1
#define EV_USE_NANOSLEEP 1
#define EV_USE_EVENTFD   1

#include <ev++.h>

namespace cocaine { namespace io {

struct service_t:
    boost::noncopyable
{
    service_t():
        m_loop(new ev::dynamic_loop())
    { }

    void
    run() {
        m_loop->loop();
    }

    void
    run(float timeout) {
        ev::timer timer(loop());

        timer.set<service_t, &service_t::on_timeout>(this);
        timer.start(timeout);

        run();
    }

    void
    stop() {
        m_loop->unloop(ev::ALL);
    }

    // Lockable concept implementation

    void
    lock() {
        m_mutex.lock();
    }

    void
    unlock() {
        m_mutex.unlock();
    }

public:
    ev::loop_ref&
    loop() {
        return *m_loop;
    }

    const ev::loop_ref&
    loop() const {
        return *m_loop;
    }

private:
    void
    on_timeout(ev::timer&, int) {
        stop();
        throw cocaine::error_t("operation has timed out");
    }

private:
    std::unique_ptr<ev::loop_ref> m_loop;

    // NOTE: Rumor says the event loop has to be interlocked for watcher
    // operations, but for some reason it works fine without it.
    std::mutex m_mutex;
};

}} // namespace cocaine::io

#endif
