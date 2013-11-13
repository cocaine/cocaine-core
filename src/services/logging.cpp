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

#include "cocaine/services/logging.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/traits/enum.hpp"

using namespace cocaine::io;
using namespace cocaine::service;

using namespace std::placeholders;

logging_t::logging_t(context_t& context, reactor_t& reactor, const std::string& name, const Json::Value& args):
    api::service_t(context, reactor, name, args),
    implementation<io::logging_tag>(context, name)
{
    auto logger = std::ref(context.logger());

    using cocaine::logging::logger_concept_t;

    on<io::logging::emit>(std::bind(&logger_concept_t::emit, logger, _1, _2, _3));
    on<io::logging::verbosity>(std::bind(&logger_concept_t::verbosity, logger));
}

dispatch_t&
logging_t::prototype() {
    return *this;
}