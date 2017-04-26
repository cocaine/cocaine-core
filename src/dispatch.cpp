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

#include "cocaine/rpc/basic_dispatch.hpp"

#include "cocaine/errors.hpp"

using namespace cocaine::io;

basic_dispatch_t::basic_dispatch_t(const std::string& name):
    m_name(name)
{ }

basic_dispatch_t::~basic_dispatch_t() {
    // Empty.
}

auto
basic_dispatch_t::attached(std::shared_ptr<session_t>) -> void {}

void
basic_dispatch_t::discard(const std::error_code& COCAINE_UNUSED_(ec)) {
    // Empty.
}

std::string
basic_dispatch_t::name() const {
    return m_name;
}
