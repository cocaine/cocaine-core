/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include <blackhole/keyword.hpp>

DECLARE_KEYWORD(source, std::string)

#define COCAINE_LOG(_log_, _level_, ...) \
    if(::blackhole::log::record_t record = _log_->logger().open_record(_level_)) \
        ::blackhole::aux::make_scoped_pump(_log_->logger(), record, __VA_ARGS__) \
        (::blackhole::keyword::source() = _log_->source())

#define COCAINE_LOG_DEBUG(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::debug, __VA_ARGS__)

#define COCAINE_LOG_INFO(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::info, __VA_ARGS__)

#define COCAINE_LOG_WARNING(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::warning, __VA_ARGS__)

#define COCAINE_LOG_ERROR(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::error, __VA_ARGS__)

#define BOOST_BIND_NO_PLACEHOLDERS
#include <blackhole/blackhole.hpp>
#include <blackhole/synchronized.hpp>

namespace cocaine { namespace logging {

struct log_context_t {
    COCAINE_DECLARE_NONCOPYABLE(log_context_t)

    log_context_t();
    log_context_t(blackhole::synchronized<logger_t>&& logger);
    log_context_t(log_context_t&& other);

    log_context_t&
    operator=(log_context_t&& other);

    priorities
    verbosity() const {
        return m_verbosity;
    }

    void
    set_verbosity(priorities value);

    blackhole::synchronized<logger_t>&
    logger() {
        return m_logger;
    }

    void
    emit(priorities level,
         const std::string& source,
         const std::string& message,
         const blackhole::log::attributes_t& attributes);

private:
    priorities m_verbosity;
    blackhole::synchronized<logger_t> m_logger;
};

struct log_t {
    log_t(context_t& context, const std::string& source);

    const std::string&
    source() const {
        return m_source;
    }

    blackhole::synchronized<logger_t>&
    logger() {
        return m_guard.logger();
    }

private:
    // The name of this log, to be used as the logging source.
    const std::string m_source;

    // Logger implementation reference.
    log_context_t& m_guard;
};

}} // namespace cocaine::logging

#endif
