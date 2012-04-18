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

#include <boost/algorithm/string/replace.hpp>
#include <syslog.h>

#include "cocaine/loggers/syslog.hpp"

using namespace cocaine::logging;

syslog_t::syslog_t(priorities verbosity, const std::string& identity):
    sink_t(verbosity),
    m_identity(identity)
{
    openlog(m_identity.c_str(), LOG_PID, LOG_USER);
}

void syslog_t::emit(priorities priority, const std::string& message) const {
    // NOTE: Replacing all newlines with spaces here because certain sysloggers
    // fail miserably interpreting them correctly.
    std::string m = boost::algorithm::replace_all_copy(message, "\n", " ");

    switch(priority) {
        case debug:
            syslog(LOG_DEBUG, "%s", m.c_str());
            break;
        
        case info:
            syslog(LOG_INFO, "%s", m.c_str());
            break;
        
        case warning:
            syslog(LOG_WARNING, "%s", m.c_str());
            break;
        
        case error:
            syslog(LOG_ERR, "%s", m.c_str());
            break;
        
        default:
            break;
    }
}
