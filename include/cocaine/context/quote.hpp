/*
    Copyright (c) 2016+ Anton Matveenko <antmat@me.com>
    Copyright (c) 2016+ Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_CONTEXT_QUOTE_HPP
#define COCAINE_CONTEXT_QUOTE_HPP

#include "cocaine/forwards.hpp"

#include <asio/ip/tcp.hpp>
#include <vector>

namespace cocaine { namespace context {

struct quote_t {
    std::vector<asio::ip::tcp::endpoint> endpoints;
    io::dispatch_ptr_t prototype;
};

}} // namespace cocaine::context
#endif
