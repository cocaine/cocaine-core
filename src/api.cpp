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

#include "cocaine/context.hpp"

#include "cocaine/api/logger.hpp"
#include "cocaine/api/service.hpp"
#include "cocaine/api/storage.hpp"

namespace cocaine { namespace api {

typedef config_t::component_map_t::const_iterator const_component_iterator;

category_traits<logger_t>::ptr_type
logger(context_t& context,
       const std::string& name)
{
    const_component_iterator it = context.config.loggers.find(name);

    if(it == context.config.loggers.end()) {
        throw configuration_error_t("the '%s' logger is not configured", name);
    }

    return context.get<logger_t>(
        it->second.type,
        context,
        cocaine::format("logger/%s", name),
        it->second.args
    );
}

category_traits<service_t>::ptr_type
service(context_t& context,
        const std::string& name)
{
    const_component_iterator it = context.config.services.find(name);

    if(it == context.config.services.end()) {
        throw configuration_error_t("the '%s' service is not configured", name);
    }

    return context.get<service_t>(
        it->second.type,
        context,
        name,
        it->second.args
    );
}

category_traits<storage_t>::ptr_type
storage(context_t& context,
        const std::string& name)
{
    const_component_iterator it = context.config.storages.find(name);

    if(it == context.config.storages.end()) {
        throw configuration_error_t("the '%s' storage is not configured", name);
    }

    return context.get<storage_t>(
        it->second.type,
        context,
        cocaine::format("storage/%s", name),
        it->second.args
    );
}

}} // namespace cocaine::api
