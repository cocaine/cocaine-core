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

// Logging macros

#define COCAINE_LOG_DEBUG(log, format, ...)                 \
    if(log->verbosity() >= logging::debug) {                \
        log->emit(logging::debug, format, ##__VA_ARGS__);   \
    }

#define COCAINE_LOG_INFO(log, format, ...)                  \
    if(log->verbosity() >= logging::info)  {                \
        log->emit(logging::info, format, ##__VA_ARGS__);    \
    }

#define COCAINE_LOG_WARNING(log, format, ...)               \
    if(log->verbosity() >= logging::warning) {              \
        log->emit(logging::warning, format, ##__VA_ARGS__); \
    }

#define COCAINE_LOG_ERROR(log, format, ...)                 \
    if(log->verbosity() >= logging::error) {                \
        log->emit(logging::error, format, ##__VA_ARGS__);   \
    }

namespace cocaine { namespace logging {

enum priorities {
    ignore,
    error,
    warning,
    info,
    debug
};

class sink_t {
    public:
        sink_t(priorities verbosity);

        virtual
        ~sink_t();

        priorities
        verbosity() const {
            return m_verbosity;
        }

        virtual
        void
        emit(priorities priority,
             const std::string& source,
             const std::string& message) const = 0;

    private:
        const priorities m_verbosity;
};

class logger_t:
    public boost::noncopyable
{
    public:
        logger_t(const sink_t& sink,
                 const std::string& source);

        priorities
        verbosity() const;

        template<typename... Args>
        void
        emit(priorities priority,
             const std::string& format,
             const Args&... args) const
        {
            boost::format message(format);

            try {
                // Recursively expand the argument pack.
                substitute(message, args...);
            } catch(const boost::io::format_error& e) {
                m_sink.emit(priority, m_source, "<unable to format the log message>");
                return;
            }

            m_sink.emit(priority, m_source, message.str());
        }

    private:
        template<typename T, typename... Args>
        void
        substitute(boost::format& message,
                   const T& argument,
                   const Args&... args) const
        {
            substitute(message % argument, args...);
        }

        void
        substitute(boost::format&) const {
            return;
        }

    private:
        const sink_t& m_sink;
        const std::string m_source;
};

class void_sink_t:
    public sink_t
{
    public:
        void_sink_t();

        virtual
        void
        emit(priorities,
             const std::string&,
             const std::string&) const;
};

}} // namespace cocaine::logging

#endif
