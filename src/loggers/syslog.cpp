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

#include "cocaine/detail/loggers/syslog.hpp"

#include <syslog.h>

using namespace cocaine::logger;

syslog_t::syslog_t(const Json::Value& args):
    category_type(args),
    m_identity(args["identity"].asString())
{
    if(m_identity.empty()) {
        throw configuration_error_t("no syslog identity has been specified");
    }

    openlog(m_identity.c_str(), LOG_PID, LOG_USER);
}

void
syslog_t::emit(logging::priorities priority,
               const std::string& source,
               const std::string& message)
{
    switch(priority) {
        case logging::debug:
            syslog(LOG_DEBUG, "%s: %s", source.c_str(), message.c_str());
            break;

        case logging::info:
            syslog(LOG_INFO, "%s: %s", source.c_str(), message.c_str());
            break;

        case logging::warning:
            syslog(LOG_WARNING, "%s: %s", source.c_str(), message.c_str());
            break;

        case logging::error:
            syslog(LOG_ERR, "%s: %s", source.c_str(), message.c_str());
            break;

        default:
            break;
    }
}
