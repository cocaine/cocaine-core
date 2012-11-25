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

#include "cocaine/common.hpp"

#include "cocaine/api/sink.hpp"

#include "cocaine/helpers/format.hpp"

// Logging macros

#define COCAINE_LOG(l, v, ...)      \
    if(l->verbosity() >= v) {       \
        l->emit(v, __VA_ARGS__);    \
    }

#define COCAINE_LOG_DEBUG(log, ...)                 \
    COCAINE_LOG(log, logging::debug, __VA_ARGS__)

#define COCAINE_LOG_INFO(log, ...)                  \
    COCAINE_LOG(log, logging::info, __VA_ARGS__)

#define COCAINE_LOG_WARNING(log, ...)               \
    COCAINE_LOG(log, logging::warning, __VA_ARGS__)

#define COCAINE_LOG_ERROR(log, ...)                 \
    COCAINE_LOG(log, logging::error, __VA_ARGS__)

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
            m_sink.emit(priority, m_source, cocaine::format(format, args...));
        }

    private:
        api::sink_t& m_sink;
        
        // This is the logging source component name, so that log messages could
        // be processed based on where they came from.
        const std::string m_source;
};

}} // namespace cocaine::logging

#endif
