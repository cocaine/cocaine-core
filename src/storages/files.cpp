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

#include "cocaine/detail/storages/files.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>

#include <boost/iterator/filter_iterator.hpp>

using namespace cocaine::storage;

namespace fs = boost::filesystem;

files_t::files_t(context_t& context,
                 const std::string& name,
                 const Json::Value& args):
    category_type(context, name, args),
    m_log(new logging::log_t(context, name)),
    m_storage_path(args["path"].asString())
{ }

files_t::~files_t() {
    // Empty.
}

std::string
files_t::read(const std::string& collection,
              const std::string& key)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    const fs::path file_path(m_storage_path / collection / key);

    fs::ifstream stream(file_path);

    COCAINE_LOG_DEBUG(
        m_log,
        "reading the '%s' object, collection: '%s', path: '%s'",
        key,
        collection,
        file_path.string()
    );

    if(!stream) {
        throw storage_error_t("the specified object has not been found");
    }

    std::stringstream buffer;
    buffer << stream.rdbuf();

    return buffer.str();
}

void
files_t::write(const std::string& collection,
               const std::string& key,
               const std::string& blob)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    const fs::path store_path(m_storage_path / collection);
    const auto store_status = fs::status(store_path);

    if(!fs::exists(store_status)) {
        COCAINE_LOG_INFO(
            m_log,
            "creating collection: %s, path: '%s'",
            collection,
            store_path.string()
        );

        try {
            fs::create_directories(store_path);
        } catch(const fs::filesystem_error& e) {
            throw storage_error_t("cannot create the specified collection");
        }
    } else if(!fs::is_directory(store_status)) {
        throw storage_error_t("the specified collection is corrupted");
    }

    const fs::path file_path(store_path / key);

    fs::ofstream stream(
        file_path,
        fs::ofstream::out | fs::ofstream::trunc
    );

    COCAINE_LOG_DEBUG(
        m_log,
        "writing the '%s' object, collection: '%s', path: '%s'",
        key,
        collection,
        file_path.string()
    );

    if(!stream) {
        throw storage_error_t("unable to access the specified object");
    }

    stream << blob;
    stream.close();
}

namespace {
    struct validate_t {
        template<typename T>
        bool
        operator()(const T& entry) const {
            return fs::is_regular_file(entry);
        }
    };
}

std::vector<std::string>
files_t::list(const std::string& collection) {
    std::unique_lock<std::mutex> lock(m_mutex);

    const fs::path store_path(m_storage_path / collection);

    std::vector<std::string> result;

    if(!fs::exists(store_path)) {
        return result;
    }

    typedef boost::filter_iterator<
        validate_t,
        fs::directory_iterator
    > file_iterator;

    file_iterator it = file_iterator(validate_t(), fs::directory_iterator(store_path)),
                  end;

    while(it != end) {
#if BOOST_VERSION >= 104600
        result.emplace_back(it->path().filename().string());
#else
        result.emplace_back(it->path().filename());
#endif

        ++it;
    }

    return result;
}

void
files_t::remove(const std::string& collection,
                const std::string& key)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    const fs::path file_path(m_storage_path / collection / key);

    if(fs::exists(file_path)) {
        COCAINE_LOG_DEBUG(
            m_log,
            "removing the '%s' object, collection: '%s', path: %s",
            key,
            collection,
            file_path.string()
        );

        try {
            fs::remove(file_path);
        } catch(const fs::filesystem_error& e) {
            throw storage_error_t("unable to remove the specified object");
        }
    }
}
