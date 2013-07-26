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

#ifndef COCAINE_FILE_STORAGE_HPP
#define COCAINE_FILE_STORAGE_HPP

#include "cocaine/api/storage.hpp"

#include <boost/filesystem/path.hpp>

namespace cocaine { namespace storage {

class files_t:
    public api::storage_t
{
    public:
        files_t(context_t& context, const std::string& name, const Json::Value& args);

        virtual
       ~files_t();

        virtual
        std::string
        read(const std::string& collection, const std::string& key);

        virtual
        void
        write(const std::string& collection, const std::string& key, const std::string& blob, const std::vector<std::string>& tags);

        virtual
        void
        remove(const std::string& collection, const std::string& key);

        virtual
        std::vector<std::string>
        find(const std::string& collection, const std::vector<std::string>& tags);

    private:
        const std::unique_ptr<logging::log_t> m_log;

        // TODO: Drop this, switch to atomic writes.
        std::mutex m_mutex;

        const boost::filesystem::path m_storage_path;
};

}} // namespace cocaine::storage

#endif
