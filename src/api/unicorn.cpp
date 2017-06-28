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

#include "cocaine/api/unicorn.hpp"
#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/repository/unicorn.hpp"

#include <boost/optional/optional.hpp>

namespace cocaine { namespace api {

auto_scope_t::auto_scope_t(unicorn_scope_ptr wrapped) :
    wrapped(std::move(wrapped))
{}

auto_scope_t::auto_scope_t(auto_scope_t&& other) :
    wrapped(std::move(other.wrapped))
{}

auto_scope_t& auto_scope_t::operator=(auto_scope_t&& other)
{
    close();
    wrapped = std::move(other.wrapped);
    return *this;
}

auto_scope_t::~auto_scope_t() {
    close();
}

auto auto_scope_t::close() -> void {
    if(wrapped){
        wrapped->close();
    }
}

// Unicorn

unicorn_t::unicorn_t(context_t& /*context*/, const std::string& /*name*/, const dynamic_t& /*args*/) {
    // empty
}

/**
 * Unicorn trait for service creation.
 * Trait to create unicorn service by name. All instances are cached by name as it is done in storage.
 */
unicorn_ptr
unicorn(context_t& context, const std::string& name) {
    auto unicorn = context.config().unicorns().get(name);
    if(!unicorn) {
        throw error_t(error::component_not_found, "unicorn component \"{}\" not found in the config", name);
    }
    return context.repository().get<unicorn_t>(unicorn->type(), context, name, unicorn->args());
}

}} // namespace cocaine::api
