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

#ifndef COCAINE_HELPERS_PID_FILE_HPP
#define COCAINE_HELPERS_PID_FILE_HPP

#include "cocaine/common.hpp"

#include <csignal>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <sys/types.h>

namespace cocaine {

class pid_file_t:
    public boost::noncopyable
{
    public:
        pid_file_t(const boost::filesystem::path& filepath):
            m_filepath(filepath)
        {
            // If the pidfile exists, check if the process is still active.
            if(boost::filesystem::exists(m_filepath)) {
                pid_t pid;
                boost::filesystem::ifstream stream(m_filepath);

                if(stream) {
                    stream >> pid;

                    if(::kill(pid, 0) < 0 && errno == ESRCH) {
                        // Unlink the stale pid file.
                        remove();
                    } else {
                        throw cocaine::error_t("another process is active");
                    }
                } else {
                    throw cocaine::error_t("unable to read '%s'", m_filepath.string());
                }
            }

            boost::filesystem::ofstream stream(m_filepath);

            if(!stream) {
                throw cocaine::error_t("unable to write '%s'", m_filepath.string());
            }

            stream << ::getpid();
            stream.close();
        }

        ~pid_file_t() {
            try {
                remove();
            } catch(...) {
                // Do nothing.
            }
        }

    private:
        void
        remove() {
            try {
                boost::filesystem::remove(m_filepath);
            } catch(const boost::filesystem::filesystem_error& e) {
                throw cocaine::error_t("unable to remove '%s'", m_filepath.string());
            }
        }

    private:
        const boost::filesystem::path m_filepath;
};

} // namespace cocaine

#endif
