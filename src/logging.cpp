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

#include <boost/tuple/tuple.hpp>
#include <cstdio>

#include "cocaine/logging.hpp"

using namespace cocaine::logging;

// Loggers
// -------

logger_t::logger_t(sink_t& sink, const std::string& name):
    m_sink(sink),
    m_name(name)
{
    memset(m_buffer, 0, LOG_BUFFER_SIZE);
}

void logger_t::debug(const char * format, ...) const {
    va_list args;
    va_start(args, format);
    emit(logging::debug, format, args);
    va_end(args);
}

void logger_t::info(const char * format, ...) const {
    va_list args;
    va_start(args, format);
    emit(logging::info, format, args);
    va_end(args);
}

void logger_t::warning(const char * format, ...) const {
    va_list args;
    va_start(args, format);
    emit(logging::warning, format, args);
    va_end(args);
}

void logger_t::error(const char * format, ...) const {
    va_list args;
    va_start(args, format);
    emit(logging::error, format, args);
    va_end(args);
}

void logger_t::emit(priorities priority, const char * format, va_list args) const {
    if(m_sink.ignores(priority)) {
        return;
    }

    boost::lock_guard<boost::mutex> lock(m_mutex);

    vsnprintf(m_buffer, LOG_BUFFER_SIZE, format, args);
    
    m_sink.emit(priority, m_name + ": " + m_buffer);
}

// Logging sinks
// -------------

sink_t::sink_t(priorities verbosity):
    m_verbosity(verbosity)
{ }

sink_t::~sink_t() { }

boost::shared_ptr<logger_t> sink_t::get(const std::string& name) {
    boost::lock_guard<boost::mutex> lock(m_mutex);

    logger_map_t::iterator it(m_loggers.find(name));

    if(it == m_loggers.end()) {
        boost::tie(it, boost::tuples::ignore) = m_loggers.insert(
            std::make_pair(
                name,
                boost::make_shared<logger_t>(
                    boost::ref(*this), 
                    name
                )
            )
        );
    }

    return it->second;
}

// Void logger
// -----------

void_sink_t::void_sink_t():
    sink_t(ignore)
{ }

void void_sink_t::emit(priorities, const std::string&) const { }
