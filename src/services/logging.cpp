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

#include "cocaine/detail/services/logging.hpp"

#include "cocaine/api/logger.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/traits/enum.hpp"

using namespace cocaine::io;
using namespace cocaine::logging;
using namespace cocaine::service;

using namespace std::placeholders;

logging_t::logging_t(context_t& context, reactor_t& reactor, const std::string& name, const dynamic_t& args):
    api::service_t(context, reactor, name, args),
    implements<io::log_tag>(context, name)
{
    auto backend = args.as_object().at("backend", "core").as_string();

    if(backend != "core") {
        const auto it = context.config.loggers.find(backend);

        if(it == context.config.loggers.end()) {
            throw cocaine::error_t("the '%s' logger is not configured", backend);
        }

        m_logger = context.get<api::logger_t>(it->second.type, context.config, it->second.args);
    }

    logger_concept_t* ptr = m_logger ? m_logger.get() : &context.logger();

    on<io::log::emit>(std::bind(&logger_concept_t::emit, ptr, _1, _2, _3));
    on<io::log::verbosity>(std::bind(&logger_concept_t::verbosity, ptr));
}

auto
logging_t::prototype() -> dispatch_t& {
    return *this;
}
