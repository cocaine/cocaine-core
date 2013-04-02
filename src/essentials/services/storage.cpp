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

#include "cocaine/essentials/services/storage.hpp"

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
    on<storage::read  >("read",   std::bind(&api::storage_t::read,   m_storage, _1, _2));
    on<storage::write >("write",  std::bind(&api::storage_t::write,  m_storage, _1, _2, _3));
    on<storage::remove>("remove", std::bind(&api::storage_t::remove, m_storage, _1, _2));
    on<storage::list  >("list",   std::bind(&api::storage_t::list,   m_storage, _1));
}
