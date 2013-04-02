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

#ifndef COCAINE_PID_FILE_HPP
#define COCAINE_PID_FILE_HPP

#include "cocaine/common.hpp"

#include <boost/filesystem/path.hpp>

namespace cocaine {

class pid_file_t:
    boost::noncopyable
{
    public:
        pid_file_t(const boost::filesystem::path& filepath);
       ~pid_file_t();

    private:
        void
        remove();

    private:
        const boost::filesystem::path m_filepath;
};

} // namespace cocaine

#endif
