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
#include "cocaine/format.hpp"

#define COCAINE_LOG(_log_, _level_, ...) \
    if(_log_->verbosity() >= _level_) _log_->emit(_level_, __VA_ARGS__);

#define COCAINE_LOG_DEBUG(_log_, ...) \
    COCAINE_LOG(_log_, logging::debug, __VA_ARGS__)

#define COCAINE_LOG_INFO(_log_, ...) \
    COCAINE_LOG(_log_, logging::info, __VA_ARGS__)

#define COCAINE_LOG_WARNING(_log_, ...) \
    COCAINE_LOG(_log_, logging::warning, __VA_ARGS__)

#define COCAINE_LOG_ERROR(_log_, ...) \
    COCAINE_LOG(_log_, logging::error, __VA_ARGS__)

namespace cocaine { namespace logging {

struct logger_concept_t {
    virtual
   ~logger_concept_t() {
        // Empty.
    }

    virtual
    logging::priorities
    verbosity() const = 0;

    virtual
    void
    emit(priorities priority,
         const std::string& source,
         const std::string& message) = 0;
};

struct log_t {
    log_t(context_t& context,
          const std::string& source);

    priorities
    verbosity() const {
        return m_logger.verbosity();
    }

    template<typename... Args>
    void
    emit(priorities level,
         const std::string& format,
         const Args&... args)
    {
        m_logger.emit(level, m_source, cocaine::format(format, args...));
    }

    void
    emit(priorities level,
         const std::string& message)
    {
        m_logger.emit(level, m_source, message);
    }

private:
    logger_concept_t& m_logger;

    // The name of this log, to be used as the logging source.
    const std::string m_source;
};

}}

#endif
