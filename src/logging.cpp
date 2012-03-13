//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <boost/tuple/tuple.hpp>
#include <cstdio>

#include "cocaine/logging.hpp"

using namespace cocaine::logging;

// Loggers
// -------

logger_t::logger_t(sink_t& sink, const std::string& name):
    m_sink(sink),
    m_name(name)
{ }

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
    vsnprintf(m_buffer, LOG_BUFFER_SIZE, format, args);
    m_sink.emit(priority, m_name + ": " + m_buffer);
}

// Logging sinks
// -------------

sink_t::~sink_t() { }

boost::shared_ptr<logger_t> sink_t::get(const std::string& name) {
    boost::lock_guard<boost::mutex> lock(m_mutex);

    logger_map_t::iterator it(m_loggers.find(name));

    if(it != m_loggers.end()) {
        return it->second;
    }

    boost::tie(it, boost::tuples::ignore) = m_loggers.insert(
        std::make_pair(
            name,
            boost::make_shared<logger_t>(
                boost::ref(*this), 
                name
            )
        )
    );

    return it->second;
}

// Void logger
// -----------

void void_sink_t::emit(priorities, const std::string&) const
{ }
