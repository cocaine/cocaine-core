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

#include <boost/algorithm/string/replace.hpp>
#include <syslog.h>

#include "cocaine/server/syslog.hpp"

using namespace cocaine::logging;

syslog_t::syslog_t(priorities verbosity, const std::string& identity):
    sink_t(verbosity),
    m_identity(identity)
{
    openlog(m_identity.c_str(), LOG_PID, LOG_USER);
}

void syslog_t::emit(priorities priority,
                    const std::string& source,
                    const std::string& message) const
{
    // NOTE: Replacing all newlines with spaces here because certain sysloggers
    // fail miserably interpreting them correctly.
    std::string m(boost::algorithm::replace_all_copy(message, "\n", " "));

    switch(priority) {
        case debug:
            syslog(LOG_DEBUG, "%s: %s", source.c_str(), m.c_str());
            break;
        
        case info:
            syslog(LOG_INFO, "%s: %s", source.c_str(), m.c_str());
            break;
        
        case warning:
            syslog(LOG_WARNING, "%s: %s", source.c_str(), m.c_str());
            break;
        
        case error:
            syslog(LOG_ERR, "%s: %s", source.c_str(), m.c_str());
            break;
        
        default:
            break;
    }
}
