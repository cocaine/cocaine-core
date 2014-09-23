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

#include "cocaine/context.hpp"
#include "cocaine/messages.hpp"
#include "cocaine/traits/enum.hpp"

#include <cerrno>
#include <cstring>
#include <ctime>
#include <system_error>

#include <sys/uio.h>

using namespace cocaine::logger;
using namespace cocaine::service;

using namespace std::placeholders;

files_t::files_t(const config_t& config, const Json::Value& args):
    category_type(config, args),
    m_path(args["path"].asString()),
    m_file(nullptr)
{
    m_file = std::fopen(m_path.c_str(), "a");

    if(m_file == nullptr) {
        throw std::system_error(errno, std::system_category(), cocaine::format("unable to open '%s'", m_path));
    }
}

files_t::~files_t() {
    if(m_file) {
        std::fclose(m_file);
    }
}

namespace {

const char* describe[] = {
    nullptr,
    "ERROR",
    "WARNING",
    "INFO",
    "DEBUG"
};

}

void
files_t::emit(logging::priorities priority, const std::string& source, const std::string& message) {
    time_t time = 0;
    tm timeinfo;

    std::memset(&timeinfo, 0, sizeof(timeinfo));

    std::time(&time);
    ::localtime_r(&time, &timeinfo);

    char timestamp[128];

    if(std::strftime(timestamp, 128, "%c", &timeinfo) == 0) {
        return;
    }

    const std::string out = cocaine::format(
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

void
files_t::reopen() {
    FILE* oldfile = m_file;
    FILE* newfile = std::fopen(m_path.c_str(), "a");
    if (!newfile) {
        throw std::ios_base::failure(
            cocaine::format("failed to open log file '%s': %s", m_path, ::strerror(errno))
        );
    }
    m_file = newfile;

    if (oldfile) {
        std::fclose(oldfile);
    }
}

using namespace cocaine;

namespace cocaine { namespace api {

category_traits<logger_t>::ptr_type
logger(context_t& context, const std::string& name) {
    const auto it = context.config.loggers.find(name);

    if(it == context.config.loggers.end()) {
        throw repository_error_t("the '%s' logger is not configured", name);
    }

    return context.get<logger_t>(it->second.type, context.config, it->second.args);
}

}} // namespace cocaine::api

file_logger_t::file_logger_t(context_t& context, io::reactor_t& reactor, const std::string& name, const Json::Value& args) :
    api::service_t(context, reactor, name, args),
    m_underlying(api::logger(context, args.get("source", args["name"].asString()).asString())),
    m_logger(dynamic_cast<logger::files_t*>(m_underlying.get()))
{
    if (!m_logger) {
        throw error_t("underlying logger must be a file logger");
    }

    on<io::logging::emit>("emit", std::bind(&files_t::emit, m_logger, _1, _2, _3));
    on<io::logging::verbosity>("verbosity", std::bind(&files_t::verbosity, m_logger));
    on<io::logging::reopen>("reopen", std::bind(&files_t::reopen, m_logger));
}
