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

#include "cocaine/essentials/file_logger.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <sys/uio.h>

using namespace cocaine;
using namespace cocaine::logger;

files_t::files_t(context_t& context,
                 const std::string& name,
                 const Json::Value& args):
    category_type(context, name, args),
    m_file(NULL)
{
    std::string path = args["path"].asString();

    m_file = std::fopen(path.c_str(), "a");
    
    if(m_file == NULL) {
        char buffer[1024],
             * message;

#ifdef _GNU_SOURCE
        message = ::strerror_r(errno, buffer, 1024);
#else
        ::strerror_r(errno, buffer, 1024);

        // NOTE: XSI-compliant strerror_r() returns int instead of the
        // string buffer, so complete the job manually.
        message = buffer;
#endif
        
        throw cocaine::error_t("unable to open the '%s' log file - %s", path, message);
    }
}

files_t::~files_t() {
    if(m_file) {
        std::fclose(m_file);
    }
}

namespace {
    static const char * describe[] = {
        NULL,
        "ERROR",
        "WARNING",
        "INFO",
        "DEBUG"
    };
}

void
files_t::emit(logging::priorities priority,
              const std::string& source,
              const std::string& message)
{
    time_t time = 0;
    tm timeinfo;

    // XXX: Not sure if it's needed.
    std::memset(&timeinfo, 0, sizeof(timeinfo));

    std::time(&time);
    ::localtime_r(&time, &timeinfo);

    char timestamp[128];

    size_t result = std::strftime(timestamp, 128, "%c", &timeinfo);

    BOOST_ASSERT(result != 0);

    std::string out = cocaine::format(
        "[%s] [%s] %s: %s\n",
        timestamp,
        describe[priority],
        source,
        message
    );

    char * buffer = new char[out.size()];

    std::memcpy(
        buffer,
        out.data(),
        out.size()
    );

    iovec io[] = {
        { buffer, out.size() }
    };

    ssize_t written = ::writev(::fileno(m_file), io, sizeof(io) / sizeof(io[0]));

    BOOST_ASSERT(written == out.size());
}
