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

#include <numeric>

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
    std::lock_guard<std::mutex> guard(m_mutex);

    const fs::path file_path(m_storage_path / collection / key);

    if(!fs::exists(file_path)) {
        throw storage_error_t("object '%s' has not been found in '%s'", key, collection);
    }

    COCAINE_LOG_DEBUG(
        m_log,
        "reading object '%s', collection: %s, path: %s",
        key,
        collection,
        file_path
    );

    fs::ifstream stream(file_path, fs::ifstream::in | fs::ifstream::binary);

    if(!stream) {
        throw storage_error_t("unable to access object '%s' in '%s'", key, collection);
    }

    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()
    );
}

void
files_t::write(const std::string& collection, const std::string& key, const std::string& blob, const std::vector<std::string>& tags) {
    std::lock_guard<std::mutex> guard(m_mutex);

    const fs::path store_path(m_storage_path / collection);
    const auto store_status = fs::status(store_path);

    if(!fs::exists(store_status)) {
        COCAINE_LOG_INFO(m_log, "creating collection: %s, path: %s", collection, store_path);

        try {
            fs::create_directories(store_path);
        } catch(const fs::filesystem_error& e) {
            throw storage_error_t("unable to create collection '%s'", collection);
        }
    } else if(!fs::is_directory(store_status)) {
        throw storage_error_t("collection '%s' is corrupted", collection);
    }

    const fs::path file_path(store_path / key);

    COCAINE_LOG_DEBUG(
        m_log,
        "writing object '%s', collection: %s, path: %s",
        key,
        collection,
        file_path
    );

    fs::ofstream stream(file_path, fs::ofstream::out | fs::ofstream::trunc | fs::ofstream::binary);

    if(!stream) {
        throw storage_error_t("unable to access object '%s' in '%s'", key, collection);
    }

    for(auto it = tags.begin(); it != tags.end(); ++it) {
        const auto tag_path = store_path / *it;
        const auto tag_status = fs::status(tag_path);

        if(!fs::exists(tag_status)) {
            try {
                fs::create_directory(tag_path);
            } catch(const fs::filesystem_error& e) {
                throw storage_error_t("unable to create tag '%s'", *it);
            }
        } else if(!fs::is_directory(tag_status)) {
            throw storage_error_t("tag '%s' is corrupted", *it);
        }

        if(fs::is_symlink(tag_path / key)) {
            continue;
        }

        try {
            fs::create_symlink(file_path, tag_path / key);
        } catch(const fs::filesystem_error& e) {
            throw storage_error_t("unable to assign tag '%s' to object '%s' in '%s'", *it, key, collection);
        }
    }

    stream.write(blob.c_str(), blob.size());
    stream.close();
}

void
files_t::remove(const std::string& collection, const std::string& key) {
    std::lock_guard<std::mutex> guard(m_mutex);

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
            throw storage_error_t("unable to remove object '%s' from '%s'", key, collection);
        }
    }
}

namespace {

struct intersect {
    template<class T>
    T
    operator()(const T& accumulator, T& keys) const {
        T result;

        std::sort(keys.begin(), keys.end());
        std::set_intersection(accumulator.begin(), accumulator.end(), keys.begin(), keys.end(), std::back_inserter(result));

        return result;
    }
};

}

std::vector<std::string>
files_t::find(const std::string& collection, const std::vector<std::string>& tags) {
    std::lock_guard<std::mutex> guard(m_mutex);

    const fs::path store_path(m_storage_path / collection);

    if(!fs::exists(store_path) || tags.empty()) {
        return std::vector<std::string>();
    }

    std::vector<std::vector<std::string>> result;

    for(auto tag = tags.begin(); tag != tags.end(); ++tag) {
        auto tagged = result.insert(result.end(), std::vector<std::string>());

        if(!fs::exists(store_path / *tag)) {
            // If one of the tags doesn't exist, the intersection is evidently empty.
            return std::vector<std::string>();
        }

        fs::directory_iterator it(store_path / *tag), end;

        while(it != end) {
#if BOOST_VERSION >= 104600
            const std::string object = it->path().filename().string();
#else
            const std::string object = it->path().filename();
#endif

            if(!fs::exists(*it)) {
                COCAINE_LOG_DEBUG(m_log, "purging object '%s' from tag '%s'", object, *tag);

                // Remove the symlink if the object was removed.
                fs::remove(*it++);

                continue;
            }

            tagged->push_back(object);

            ++it;
        }
    }

    std::vector<std::string> initial = std::move(result.back());

    // NOTE: Pop the initial accumulator value from the result queue, so that it
    // won't be intersected with itself later.
    result.pop_back();

    // NOTE: Sort the initial accumulator value here once, because it will always
    // be kept sorted inside the functor by std::set_intersection().
    std::sort(initial.begin(), initial.end());

    return std::accumulate(result.begin(), result.end(), initial, intersect());
}
