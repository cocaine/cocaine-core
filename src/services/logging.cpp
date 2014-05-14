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

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include "cocaine/traits/attributes.hpp"
#include "cocaine/traits/enum.hpp"

using namespace cocaine::io;
using namespace cocaine::logging;
using namespace cocaine::service;

using namespace std::placeholders;

logging_t::logging_t(context_t& context,
                     reactor_t& reactor,
                     const std::string& name,
                     const dynamic_t& args):
    api::service_t(context, reactor, name, args),
    dispatch<io::log_tag>(name)
{
    auto backend = args.as_object().at("backend", "core").as_string();

    if(backend != "core") {
        try {
            auto& repository = blackhole::repository_t::instance();
            auto logger = repository.create<priorities>(backend);
            auto sync_logger = blackhole::synchronized<logger_t>(std::move(logger));
            auto log_context = log_context_t(std::move(sync_logger));
            m_logger = std::make_unique<log_context_t>(std::move(log_context));
            m_logger->set_verbosity(context.logger().verbosity());
        } catch (const std::out_of_range&) {
            throw cocaine::error_t("the '%s' logger is not configured", backend);
        }
    }

    log_context_t* log_ptr = m_logger ? m_logger.get() : &context.logger();

    on<io::log::emit>(std::bind(&log_context_t::emit, log_ptr, _1, _2, _3, _4));
    on<io::log::set_verbosity>(std::bind(&log_context_t::set_verbosity, log_ptr, _1));
    on<io::log::verbosity>(std::bind(&log_context_t::verbosity, log_ptr));
}

auto
logging_t::prototype() -> basic_dispatch_t& {
    return *this;
}
