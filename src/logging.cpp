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

#include "cocaine/logging.hpp"

using namespace cocaine::logging;

// Logging sinks

sink_t::sink_t(priorities verbosity):
    m_verbosity(verbosity)
{ }

sink_t::~sink_t() {
    // Empty.
}

// Logger

logger_t::logger_t(const sink_t& sink, const std::string& source):
    m_sink(sink),
    m_source(source)
{ }

priorities
logger_t::verbosity() const {
    return m_sink.verbosity();
}

// Void logger

void_sink_t::void_sink_t():
    sink_t(ignore)
{ }

void
void_sink_t::emit(priorities,
                  const std::string&,
                  const std::string&) const
{ }
