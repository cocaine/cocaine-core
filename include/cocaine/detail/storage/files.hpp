/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include <asio/io_service.hpp>

#include <boost/filesystem/path.hpp>
#include <boost/optional/optional.hpp>

namespace cocaine { namespace storage {

class files_t:
    public api::storage_t
{
    const std::unique_ptr<logging::logger_t> m_log;

    const boost::filesystem::path m_parent_path;

    asio::io_service io_loop;
    boost::optional<asio::io_service::work> io_work;
    std::thread thread;

public:
    files_t(context_t& context, const std::string& name, const dynamic_t& args);

    virtual
   ~files_t();

    using api::storage_t::read;

    virtual
    void
    read(const std::string& collection, const std::string& key, callback<std::string> cb);

    using api::storage_t::write;

    virtual
    void
    write(const std::string& collection,
          const std::string& key,
          const std::string& blob,
          const std::vector<std::string>& tags,
          callback<void> cb);

    using api::storage_t::remove;

    virtual
    void
    remove(const std::string& collection, const std::string& key, callback<void> cb);

    using api::storage_t::find;

    virtual
    void
    find(const std::string& collection, const std::vector<std::string>& tags, callback<std::vector<std::string>> cb);

private:
    std::string
    read_sync(const std::string& collection, const std::string& key);

    void
    write_sync(const std::string& collection,
               const std::string& key,
               const std::string& blob,
               const std::vector<std::string>& tags);

    void
    remove_sync(const std::string& collection, const std::string& key);

    std::vector<std::string>
    find_sync(const std::string& collection, const std::vector<std::string>& tags);
};

}} // namespace cocaine::storage

#endif
