/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/essentials/services/storage.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

using namespace std::placeholders;

storage_t::storage_t(context_t& context,
                     reactor_t& reactor,
                     const std::string& name,
                     const Json::Value& args):
    category_type(context, reactor, name, args),
    m_storage(api::storage(context, "core"))
{
    on<storage::read  >("read",   std::bind(&storage_t::on_read,   this, _1, _2));
    on<storage::write >("write",  std::bind(&storage_t::on_write,  this, _1, _2, _3));
    on<storage::remove>("remove", std::bind(&storage_t::on_remove, this, _1, _2));
    on<storage::list  >("list",   std::bind(&storage_t::on_list,   this, _1));
}

std::string
storage_t::on_read(const std::string& collection, const std::string& key) {
    return m_storage->read(collection, key);
}

void
storage_t::on_write(const std::string& collection, const std::string& key, const std::string& value) {
    return m_storage->write(collection, key, value);
}

void
storage_t::on_remove(const std::string& collection, const std::string& key) {
    return m_storage->remove(collection, key);
}

std::vector<std::string>
storage_t::on_list(const std::string& collection) {
    return m_storage->list(collection);
}