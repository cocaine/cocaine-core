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

#include "cocaine/detail/loggers/files.hpp"

#include <cerrno>
#include <cstring>
#include <ctime>
#include <system_error>

#include <sys/uio.h>

using namespace cocaine::logger;

files_t::files_t(const Json::Value& args):
    category_type(args),
    m_file(nullptr)
{
    std::string path = args["path"].asString();

    m_file = std::fopen(path.c_str(), "a");

    if(m_file == nullptr) {
        throw std::system_error(
            errno,
            std::system_category(),
            cocaine::format("unable to open the '%s' file", path)
        );
    }
}

files_t::~files_t() {
    if(m_file) {
        std::fclose(m_file);
    }
}

namespace {
    static
    const char* describe[] = {
        nullptr,
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

    std::memset(&timeinfo, 0, sizeof(timeinfo));

    std::time(&time);
    ::localtime_r(&time, &timeinfo);

    char timestamp[128];

    if(std::strftime(timestamp, 128, "%c", &timeinfo) == 0) {
        return;
    }

    std::string out = cocaine::format(
        "[%s] [%s] %s: %s\n",
        timestamp,
        describe[priority],
        source,
        message
    );

    char* buffer = new char[out.size()];

    std::memcpy(
        buffer,
        out.data(),
        out.size()
    );

    iovec io[] = {
        { buffer, out.size() }
    };

    if(::writev(::fileno(m_file), io, sizeof(io) / sizeof(io[0])) == -1) {
        // TODO: Do something useful.
    }

    delete[] buffer;
}
