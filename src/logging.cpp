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

#include "cocaine/logging.hpp"
#include "cocaine/context.hpp"

using namespace cocaine::logging;

log_t::log_t(context_t& context, const std::string& source):
    m_source(source),
    m_guard(context.logger())
{ }

log_context_t::log_context_t() :
    m_verbosity(priorities::ignore)
{ }

log_context_t::log_context_t(blackhole::synchronized<cocaine::logger_t>&& logger) :
    m_verbosity(priorities::ignore),
    m_logger(std::move(logger))
{ }

log_context_t::log_context_t(log_context_t&& other) :
    m_verbosity(other.m_verbosity),
    m_logger(std::move(other.m_logger))
{ }

log_context_t&
log_context_t::operator=(log_context_t&& other) {
    m_verbosity = other.m_verbosity;
    m_logger = std::move(other.m_logger);
    return *this;
}

void
log_context_t::set_verbosity(priorities value) {
    m_verbosity = value;
    m_logger.set_filter(blackhole::keyword::severity<priorities>() <= value);
}

void
log_context_t::emit(priorities level, const std::string& source, const std::string& message, const bhl::attributes_t& attributes) {
    auto record = m_logger.open_record(level);

    if(record.valid()) {
        record.attributes.insert(attributes.begin(), attributes.end());
        record.attributes.insert(blackhole::keyword::message() = message);
        record.attributes.insert(blackhole::keyword::source() = source);

        m_logger.push(std::move(record));
    }
}
