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

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/noncopyable.hpp>
#include <stdexcept>

namespace cocaine { namespace helpers {

namespace fs = boost::filesystem;

class pid_file_t:
    public boost::noncopyable
{
    public:
        pid_file_t(const std::string& filepath):
            m_filepath(filepath)
        {
            // NOTE: If the pidfile exists, check if the process is still active.
            if(fs::exists(m_filepath)) {
                pid_t pid;
                fs::ifstream stream(m_filepath);

                if(stream) {
                    stream >> pid;

                    if(kill(pid, 0) < 0 && errno == ESRCH) {
                        // NOTE: Unlink the stale pid file.
                        remove();
                    } else {
                        throw std::runtime_error("another instance is active");
                    }
                } else {
                    throw std::runtime_error("failed to read " + m_filepath.string());
                }
            }

            fs::ofstream stream(m_filepath);

            if(!stream) {
                throw std::runtime_error("failed to write " + m_filepath.string());
            }

            stream << getpid();
            stream.close();
        }

        ~pid_file_t() {
            try {
                remove();
            } catch(const std::runtime_error& e) {
                // NOTE: Do nothing.
            }
        }

    private:
        void remove() {
            try {
                fs::remove(m_filepath);
            } catch(const std::runtime_error& e) {
                throw std::runtime_error("failed to remove " + m_filepath.string());
            }
        }

    private:
        const fs::path m_filepath;
};

} // namespace helpers

using helpers::pid_file_t;

} // namespace cocaine

#endif
