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

#include "cocaine/api/storage.hpp"

#include "cocaine/detail/services/storage.hpp"

#include "cocaine/context.hpp"

using namespace cocaine::io;
using namespace cocaine::service;

storage_t::storage_t(context_t& context, reactor_t& reactor, const std::string& name, const dynamic_t& args):
    api::service_t(context, reactor, name, args),
    dispatch<io::storage_tag>(name)
{
    auto storage = api::storage(context, args.as_object().at("backend", "core").as_string());

    using namespace std::placeholders;

    on<io::storage::read>(std::bind(&api::storage_t::read, storage, _1, _2));
    on<io::storage::write>(std::bind(&api::storage_t::write, storage, _1, _2, _3, _4));
    on<io::storage::remove>(std::bind(&api::storage_t::remove, storage, _1, _2));
    on<io::storage::find>(std::bind(&api::storage_t::find, storage, _1, _2));
}

auto
storage_t::prototype() -> basic_dispatch_t& {
    return *this;
}
