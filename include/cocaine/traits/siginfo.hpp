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

#pragma once

#include "cocaine/traits.hpp"

#include <boost/assert.hpp>

#include <signal.h>

namespace cocaine { namespace io {

// Stub implementation for context disppatch with os_signal event

template<>
struct type_traits<siginfo_t> {
    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>&, siginfo_t) {
        BOOST_ASSERT(false);
    }

    static inline
    void
    unpack(const msgpack::object&, siginfo_t&) {
        BOOST_ASSERT(false);
    }
};

}} // namespace cocaine::io
