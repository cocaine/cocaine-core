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

#include "cocaine/essentials/services/locator.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::logging;
using namespace cocaine::service;

using namespace std::placeholders;

locator_t::locator_t(context_t& context,
                     reactor_t& reactor,
                     const std::string& name,
                     const Json::Value& args):
    category_type(context, reactor, name, args)
{
    on<locator::resolve>("resolve", std::bind(&locator_t::resolve, this, _1));
}

description_t
locator_t::resolve(const std::string& name) const {
    std::map<int, std::string> methods;

    methods[0] = "poke";
    methods[1] = "peek";

    description_t result = { "localhost:15000", 1, methods };

    // Do something useful.

    return result;
}
