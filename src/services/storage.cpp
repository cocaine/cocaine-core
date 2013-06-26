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

#include "cocaine/detail/services/storage.hpp"

#include "cocaine/api/storage.hpp"

#include "cocaine/context.hpp"
#include "cocaine/messages.hpp"

using namespace cocaine::service;

using namespace std::placeholders;

storage_t::storage_t(context_t& context, io::reactor_t& reactor, const std::string& name, const Json::Value& args):
    category_type(context, reactor, name, args)
{
    auto storage = api::storage(context, args.get("backend", "core").asString());

    on<io::storage::read>("read", std::bind(&api::storage_t::read, storage, _1, _2));
    on<io::storage::write>("write", std::bind(&api::storage_t::write, storage, _1, _2, _3, _4));
    on<io::storage::remove>("remove", std::bind(&api::storage_t::remove, storage, _1, _2));
    on<io::storage::find>("find", std::bind(&api::storage_t::find, storage, _1, _2));
}
