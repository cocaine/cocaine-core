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

#include <boost/thread/mutex.hpp>
#include <cstdarg>

#include "cocaine/common.hpp"

#include "cocaine/helpers/birth_control.hpp"

#define LOG_BUFFER_SIZE 50 * 1024

namespace cocaine { namespace logging {

enum priorities {
    debug,
    info,
    warning,
    error,
    ignore
};

class sink_t;

class logger_t:
    public boost::noncopyable,
    public birth_control_t<logger_t>
{
    public:
        logger_t(sink_t& sink,
                 const std::string& name);
        
        void debug(const char * format, ...) const;
        void info(const char * format, ...) const;
        void warning(const char * format, ...) const;
        void error(const char * format, ...) const;

    private:
        void emit(priorities priority,
                  const char * format, 
                  va_list args) const;

    private:
        sink_t& m_sink;
        const std::string m_name;

        mutable char m_buffer[LOG_BUFFER_SIZE];
        mutable boost::mutex m_mutex;
};

class sink_t:
    public boost::noncopyable
{
    public:
        sink_t(priorities verbosity);
        virtual ~sink_t() = 0;

        // XXX: Might be a better idea to return the logger by reference.
        boost::shared_ptr<logger_t> get(const std::string& name);

        bool ignores(priorities priority) const {
            return priority < m_verbosity;
        }
    
    public:
        virtual void emit(priorities priority,
                          const std::string& message) const = 0;

    private:
        const priorities m_verbosity;

        typedef std::map<
            const std::string,
            boost::shared_ptr<logger_t>
        > logger_map_t;

        logger_map_t m_loggers;
        boost::mutex m_mutex;
};

class void_sink_t:
    public sink_t
{
    public:
        void_sink_t();

        virtual void emit(priorities, 
                          const std::string&) const;
};

}}

#endif
