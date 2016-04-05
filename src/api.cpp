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
#include "cocaine/api/storage.hpp"
#include "cocaine/context.hpp"
#include "cocaine/context/config.hpp"
#include "cocaine/repository/storage.hpp"
#include "cocaine/repository/unicorn.hpp"

#include <boost/optional/optional.hpp>

namespace cocaine { namespace api {

// Storage

storage_ptr
storage(context_t& context, const std::string& name) {
    auto storage = context.config().storages().get(name);
    if(!storage) {
        throw std::system_error(std::make_error_code(std::errc::argument_out_of_domain), name);
    }
    return context.repository().get<storage_t>(storage->type(), context, name, storage->args());
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
        throw std::system_error(std::make_error_code(std::errc::argument_out_of_domain), name);
    }
    return context.repository().get<unicorn_t>(unicorn->type(), context, name, unicorn->args());
}

}} // namespace cocaine::api
