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

#ifndef COCAINE_IO_CHANNEL_HPP
#define COCAINE_IO_CHANNEL_HPP

#include "cocaine/asio/readable_stream.hpp"
#include "cocaine/asio/writable_stream.hpp"

#include "cocaine/rpc/decoder.hpp"
#include "cocaine/rpc/encoder.hpp"

namespace cocaine { namespace io {

template<class Socket>
struct channel {
    channel():
        rd(new decoder<readable_stream<Socket>>()),
        wr(new encoder<writable_stream<Socket>>())
    {
        // pass
    }

    channel(reactor_t& reactor, const std::shared_ptr<Socket>& socket):
        rd(new decoder<readable_stream<Socket>>()),
        wr(new encoder<writable_stream<Socket>>())
    {
        attach(reactor, socket);
    }

    void
    attach(reactor_t& reactor, const std::shared_ptr<Socket>& socket) {
        rd->attach(std::make_shared<readable_stream<Socket>>(reactor, socket));
        wr->attach(std::make_shared<writable_stream<Socket>>(reactor, socket));
    }

public:
    size_t
    footprint() const {
        return rd->stream()->footprint() +
               wr->stream()->footprint();
    }

public:
    std::unique_ptr<decoder<readable_stream<Socket>>> rd;
    std::unique_ptr<encoder<writable_stream<Socket>>> wr;
};

}}

#endif
