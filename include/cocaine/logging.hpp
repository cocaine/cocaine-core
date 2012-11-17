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

#ifndef COCAINE_LOGGING_HPP
#define COCAINE_LOGGING_HPP

#include <boost/format.hpp>

#include "cocaine/common.hpp"

#include "cocaine/api/sink.hpp"

// Logging macros

#define COCAINE_LOG_DEBUG(log, ...)                 \
    if(log->verbosity() >= logging::debug) {        \
        log->emit(logging::debug, __VA_ARGS__);     \
    }

#define COCAINE_LOG_INFO(log, ...)                  \
    if(log->verbosity() >= logging::info)  {        \
        log->emit(logging::info, __VA_ARGS__);      \
    }

#define COCAINE_LOG_WARNING(log, ...)               \
    if(log->verbosity() >= logging::warning) {      \
        log->emit(logging::warning, __VA_ARGS__);   \
    }

#define COCAINE_LOG_ERROR(log, ...)                 \
    if(log->verbosity() >= logging::error) {        \
        log->emit(logging::error, __VA_ARGS__);     \
    }

namespace cocaine { namespace logging {

class logger_t:
    public boost::noncopyable
{
    public:
        logger_t(api::sink_t& sink,
                 const std::string& source):
            m_sink(sink),
            m_source(source)
        { }

        priorities
        verbosity() const {
            return m_sink.verbosity();
        }

        template<typename... Args>
        void
        emit(priorities priority,
             const std::string& format,
             const Args&... args)
        {
            boost::format message(format);

            try {
                // NOTE: Recursively expand the argument pack.
                m_sink.emit(priority, m_source, substitute(message, args...));
            } catch(const boost::io::format_error& e) {
                m_sink.emit(priority, m_source, "<unable to format the log message>");
            }
        }

    private:
        template<typename T, typename... Args>
        static
        std::string
        substitute(boost::format& message,
                   const T& argument,
                   const Args&... args)
        {
            return substitute(message % argument, args...);
        }

        static
        std::string
        substitute(boost::format& message) {
            return message.str();
        }

    private:
        api::sink_t& m_sink;
        
        // This is the logging source component name, so that log messages could
        // be processed based on where they came from.
        const std::string m_source;
};

}} // namespace cocaine::logging

#endif
