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

#ifndef COCAINE_IO_REACTOR_HPP
#define COCAINE_IO_REACTOR_HPP

#include "cocaine/common.hpp"

#define EV_MINIMAL       0
#define EV_USE_MONOTONIC 1
#define EV_USE_REALTIME  1
#define EV_USE_NANOSLEEP 1
#define EV_USE_EVENTFD   1

#include <ev++.h>

namespace cocaine { namespace io {

struct reactor_t {
    COCAINE_DECLARE_NONCOPYABLE(reactor_t)

    typedef ev::loop_ref native_type;

    reactor_t():
        m_loop(new ev::dynamic_loop())
    { }

    void
    run() {
        m_loop->loop();
    }

    void
    stop() {
        m_loop->unloop(ev::ALL);
    }

public:
    native_type&
    native() {
        return *m_loop;
    }

private:
    std::unique_ptr<native_type> m_loop;
};

}} // namespace cocaine::io

#endif
