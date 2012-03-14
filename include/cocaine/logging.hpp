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

#ifndef COCAINE_LOGGING_HPP
#define COCAINE_LOGGING_HPP

#include <boost/thread/mutex.hpp>
#include <cstdarg>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"

#define LOG_BUFFER_SIZE 50 * 1024

namespace cocaine { namespace logging {

enum priorities {
    debug,
    info,
    warning,
    error
};

class sink_t;

class logger_t:
    public boost::noncopyable,
    public birth_control_t<logger_t>
{
    public:
        logger_t(sink_t& sink, const std::string& name);
        
        void debug(const char * format, ...) const;
        void info(const char * format, ...) const;
        void warning(const char * format, ...) const;
        void error(const char * format, ...) const;

    private:
        void emit(priorities priority, const char * format, va_list args) const;

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
        virtual ~sink_t();

        // XXX: Might be a better idea to return the logger by reference.
        boost::shared_ptr<logger_t> get(const std::string& name);
    
    public:
        virtual void emit(priorities priority, const std::string& message) const = 0;

    private:
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
        virtual void emit(priorities, const std::string&) const;
};

}}

#endif
