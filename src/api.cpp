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

namespace cocaine { namespace api {

// Storage

category_traits<storage_t>::ptr_type
storage(context_t& context, const std::string& name) {
    auto it = context.config.storages.find(name);

    if(it == context.config.storages.end()) {
        throw std::system_error(std::make_error_code(std::errc::argument_out_of_domain), name);
    }
    return context.get<storage_t>(it->second.type, context, name, it->second.args);
}

// Unicorn

unicorn_t::unicorn_t(context_t& /*context*/, const std::string& /*name*/, const dynamic_t& /*args*/) {
    // empty
}

/**
 * Unicorn trait for service creation.
 * Trait to create unicorn service by name. All instances are cached by name as it is done in storage.
 */
category_traits<unicorn_t>::ptr_type
unicorn(context_t& context, const std::string& name) {
    auto it = context.config.unicorns.find(name);

    if(it == context.config.unicorns.end()) {
        throw std::system_error(std::make_error_code(std::errc::argument_out_of_domain), name);
    }

    return context.get<unicorn_t>(it->second.type, context, name, it->second.args);
}

}} // namespace cocaine::api
