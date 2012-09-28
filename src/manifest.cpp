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

#include <boost/filesystem/operations.hpp>

#include "cocaine/manifest.hpp"

using namespace cocaine;

namespace fs = boost::filesystem;

manifest_t::manifest_t(context_t& context, const std::string& name_):
    cached<Json::Value>(context, "manifests", name_)
{
    const Json::Value cache(object());

    // Common settings.
    name = name_;
    type = cache["type"].asString();
   
    // App host type.
    slave = cache["engine"].get(
        "slave",
        defaults::slave
    ).asString();

    if(!fs::exists(fs::system_complete(slave))) {
        throw configuration_error_t("the '" + slave + "' slave binary does not exist");
    }

    // Custom app configuration.
    args = cache.get("args", Json::Value());

    // Driver configuration.
    drivers = cache.get("drivers", Json::Value());
    
    // Isolation configuration.
    limits = cache.get("resource-limits", Json::Value());
}

