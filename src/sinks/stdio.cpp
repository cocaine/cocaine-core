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

#include "cocaine/sinks/stdio.hpp"

#include <ctime>

#include <boost/format.hpp>

using namespace cocaine;
using namespace cocaine::sink;

stdio_t::stdio_t(const std::string& name,
                 const Json::Value& args):
    category_type(name, args)
{ }

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
stdio_t::emit(logging::priorities priority,
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

    BOOST_ASSERT(result);

    std::cout << boost::format("[%s] [%s] %s: %s")
                    % timestamp 
                    % describe[priority] 
                    % source 
                    % message
              << std::endl;
}
