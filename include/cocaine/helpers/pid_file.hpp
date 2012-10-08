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
#include <boost/format.hpp>
#include <boost/noncopyable.hpp>
#include <stdexcept>

namespace cocaine { namespace helpers {

class pid_file_t:
    public boost::noncopyable
{
    public:
        pid_file_t(const std::string& filepath):
            m_filepath(filepath)
        {
            // NOTE: If the pidfile exists, check if the process is still active.
            if(boost::filesystem::exists(m_filepath)) {
                pid_t pid;
                boost::filesystem::ifstream stream(m_filepath);

                if(stream) {
                    stream >> pid;

                    if(::kill(pid, 0) < 0 && errno == ESRCH) {
                        // NOTE: Unlink the stale pid file.
                        remove();
                    } else {
                        throw std::runtime_error("another process is active");
                    }
                } else {
                    boost::format message("unable to read '%s'");
                    throw std::runtime_error((message % m_filepath.string()).str());
                }
            }

            boost::filesystem::ofstream stream(m_filepath);

            if(!stream) {
                boost::format message("unable to write '%s'");
                throw std::runtime_error((message % m_filepath.string()).str());
            }

            stream << ::getpid();
            stream.close();
        }

        ~pid_file_t() {
            try {
                remove();
            } catch(...) {
                // NOTE: Do nothing.
            }
        }

    private:
        void
        remove() {
            try {
                boost::filesystem::remove(m_filepath);
            } catch(const boost::filesystem::filesystem_error& e) {
                boost::format message("unable to remove '%s'");
                throw std::runtime_error((message % m_filepath.string()).str());
            }
        }

    private:
        const boost::filesystem::path m_filepath;
};

} // namespace helpers

using helpers::pid_file_t;

} // namespace cocaine

#endif
