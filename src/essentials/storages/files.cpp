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

#include "cocaine/essentials/storages/files.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>

using namespace cocaine::storage;

namespace fs = boost::filesystem;

files_t::files_t(context_t& context, const std::string& name, const Json::Value& args):
    category_type(context, name, args),
    m_log(new logging::log_t(context, name)),
    m_storage_path(args["path"].asString())
{ }

files_t::~files_t() {
    // Empty.
}

std::string
files_t::read(const std::string& collection, const std::string& key) {
    std::unique_lock<std::mutex> lock(m_mutex);

    const fs::path file_path(m_storage_path / collection / key);

    fs::ifstream stream(file_path);

    COCAINE_LOG_DEBUG(
        m_log,
        "reading object '%s', collection: %s, path: %s",
        key,
        collection,
        file_path
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
               const std::string& blob,
               const std::vector<std::string>& tags)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    const fs::path store_path(m_storage_path / collection);
    const auto store_status = fs::status(store_path);

    if(!fs::exists(store_status)) {
        COCAINE_LOG_INFO(m_log, "creating collection: %s, path: %s", collection, store_path);

        try {
            fs::create_directories(store_path);
        } catch(const fs::filesystem_error& e) {
            throw storage_error_t("unable to create the specified collection");
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
        "writing object '%s', collection: %s, path: %s",
        key,
        collection,
        file_path
    );

    if(!stream) {
        throw storage_error_t("unable to access the specified object");
    }

    stream << blob;
    stream.close();

    for(auto it = tags.begin(); it != tags.end(); ++it) {
        const auto tag_path = store_path / *it;
        const auto tag_status = fs::status(tag_path);

        if(!fs::exists(tag_status)) {
            try {
                fs::create_directory(tag_path);
            } catch(const fs::filesystem_error& e) {
                throw storage_error_t("unable to create the specified tag");
            }
        } else if(!fs::is_directory(tag_status)) {
            throw storage_error_t("the specified tag is corrupted");
        }

        if(!fs::is_symlink(tag_path / key)) {
            try {
                fs::create_symlink(file_path, tag_path / key);
            } catch(const fs::filesystem_error& e) {
                throw storage_error_t("unable to tag the specified object");
            }
        }
    }
}

void
files_t::remove(const std::string& collection, const std::string& key) {
    std::unique_lock<std::mutex> lock(m_mutex);

    const auto store_path(m_storage_path / collection);
    const auto file_path(store_path / key);

    if(fs::exists(file_path)) {
        COCAINE_LOG_DEBUG(
            m_log,
            "removing object '%s', collection: %s, path: %s",
            key,
            collection,
            file_path
        );

        try {
            fs::remove(file_path);
        } catch(const fs::filesystem_error& e) {
            throw storage_error_t("unable to remove the specified object");
        }
    }
}

std::vector<std::string>
files_t::find(const std::string& collection, const std::vector<std::string>& tags) {
    std::unique_lock<std::mutex> lock(m_mutex);

    const fs::path store_path(m_storage_path / collection);

    std::vector<std::string> result;

    if(!fs::exists(store_path)) {
        return result;
    }

    for(auto tag = tags.begin(); tag != tags.end(); ++tag) {
        if(!fs::exists(store_path / *tag)) {
            continue;
        }

        fs::directory_iterator it(store_path / *tag), end;

        while(it != end) {
#if BOOST_VERSION >= 104600
            auto object = it->path().filename().string();
#else
            auto object = it->path().filename();
#endif

            if(!fs::exists(*it)) {
                COCAINE_LOG_DEBUG(m_log, "purging object %s from tag %s", object, *tag);

                // Remove the symlink if the object was removed.
                fs::remove(*it++);

                continue;
            }

            result.emplace_back(object);

            ++it;
        }
    }

    std::sort(result.begin(), result.end());

    // Remove the duplicates, if any.
    result.erase(std::unique(result.begin(), result.end()), result.end());

    return result;
}
