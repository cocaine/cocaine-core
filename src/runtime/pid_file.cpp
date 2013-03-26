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

#include "cocaine/detail/runtime/pid_file.hpp"

#include <csignal>

#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

#include <sys/types.h>

using namespace cocaine;

namespace fs = boost::filesystem;

pid_file_t::pid_file_t(const fs::path& filepath):
    m_filepath(filepath)
{
    // If the pidfile exists, check if the process is still active.
    if(fs::exists(m_filepath)) {
        pid_t pid;
        fs::ifstream stream(m_filepath);

        if(!stream) {
            throw cocaine::error_t("unable to read '%s'", m_filepath.string());
        }

        stream >> pid;

        if(::kill(pid, 0) < 0 && errno == ESRCH) {
            // Unlink the stale pid file.
            remove();
        } else {
            throw cocaine::error_t("another process is active");
        }
    }

    fs::ofstream stream(m_filepath);

    if(!stream) {
        throw cocaine::error_t("unable to write '%s'", m_filepath.string());
    }

    stream << ::getpid();
    stream.close();
}

pid_file_t::~pid_file_t() {
    try {
        remove();
    } catch(...) {
        // Do nothing.
    }
}

void
pid_file_t::remove() {
    try {
        fs::remove(m_filepath);
    } catch(const fs::filesystem_error& e) {
        throw cocaine::error_t("unable to remove '%s'", m_filepath.string());
    }
}
