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

#include "cocaine/detail/manifest.hpp"
#include "cocaine/detail/traits/json.hpp"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

using namespace cocaine;
using namespace cocaine::engine;

namespace fs = boost::filesystem;

manifest_t::manifest_t(context_t& context,
                       const std::string& name_):
    cached<Json::Value>(context, "manifests", name_),
    name(name_)
{
    endpoint = cocaine::format(
        "%s/engines/%s",
        context.config.path.runtime,
        name
    );

    auto prefix = fs::path(context.config.path.spool) / name;

    slave = fs::complete(
        get("slave", "unspecified").asString(),
        prefix
    ).string();

    sandbox = {
        get("type", "unspecified").asString(),
        (*this)["args"]
    };

    drivers = config_t::parse((*this)["drivers"]);
}

