/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/service/logging.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/traits/attributes.hpp"
#include "cocaine/traits/enum.hpp"

using namespace blackhole;

using namespace cocaine::logging;
using namespace cocaine::service;

logging_t::logging_t(context_t& context, boost::asio::io_service& asio, const std::string& name, const dynamic_t& args):
    category_type(context, asio, name, args),
    dispatch<io::log_tag>(name)
{
    const auto backend = args.as_object().at("backend", "core").as_string();

    try {
        m_logger = std::make_unique<logger_t>(repository_t::instance().create<logger_t>(
            backend,
            context.config.logging.loggers.at(backend).verbosity
        ));
    } catch(const std::out_of_range&) {
        throw cocaine::error_t("logger '%s' is not configured", backend);
    }

    using namespace std::placeholders;

    const auto getter = &logger_t::verbosity;
    const auto setter = static_cast<void(logger_t::*)(priorities)>(&logger_t::set_filter);

    on<io::log::emit>(std::bind(&logging_t::on_emit, this, _1, _2, _3, _4));
    on<io::log::verbosity>(std::bind(getter, std::ref(*m_logger)));
    on<io::log::set_verbosity>(std::bind(setter, std::ref(*m_logger), _1));
}

auto
logging_t::prototype() const -> const basic_dispatch_t& {
    return *this;
}

void
logging_t::on_emit(logging::priorities level, std::string&& source, std::string&& message,
                   blackhole::attribute::set_t&& attributes)
{
    auto record = m_logger->open_record(level, std::move(attributes));

    if(!record.valid()) {
        return;
    }

    record.insert(cocaine::logging::keyword::source() = std::move(source));
    record.message(std::move(message));

    m_logger->push(std::move(record));
}
