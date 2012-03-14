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

#include "cocaine/logging.hpp"

using namespace cocaine::logging;

syslog_t::syslog_t(const std::string& identity, int verbosity):
    m_identity(identity)
{
    // Setting up the syslog.
    openlog(m_identity.c_str(), LOG_PID | LOG_NDELAY, LOG_USER);
    setlogmask(LOG_UPTO(verbosity));
}

void syslog_t::emit(priorities priority, const std::string& message) const {
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
    }
}
