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

#include "cocaine/detail/loggers/stdout.hpp"

#include <cstring>
#include <ctime>

using namespace cocaine::logger;

stdout_t::stdout_t(const Json::Value& args):
    category_type(args)
{ }

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
stdout_t::emit(logging::priorities priority,
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

    std::cout << cocaine::format(
        "[%s] [%s] %s: %s",
        timestamp,
        describe[priority],
        source,
        message
    );

    std::cout << std::endl;
}
