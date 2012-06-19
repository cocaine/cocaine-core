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

#ifndef COCAINE_PACKAGE_HPP
#define COCAINE_PACKAGE_HPP

#include <boost/filesystem/path.hpp>

#include "cocaine/common.hpp"

#include "cocaine/helpers/blob.hpp"

struct archive;

namespace cocaine {

struct package_error_t:
    public std::runtime_error
{
    package_error_t(archive * source);
};

class package_t {
    public:
        package_t(context_t& context,
                  const blob_t& archive);
        
        ~package_t();

        void deploy(const boost::filesystem::path& prefix);
        
    public:
        std::string type() const;

    private:
        static void extract(archive * source, 
                            archive * target);

    private:
        boost::shared_ptr<logging::logger_t> m_log;
        archive * m_archive;
};

}

#endif
