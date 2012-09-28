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

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/iterator/filter_iterator.hpp>

#include "cocaine/storages/files.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::api;
using namespace cocaine::storage;

namespace fs = boost::filesystem;

file_storage_t::file_storage_t(context_t& context, const std::string& name, const Json::Value& args):
    category_type(context, name, args),
    m_log(context.log(name)),
    m_storage_path(args["path"].asString())
{ }

std::string
file_storage_t::read(const std::string& collection,
                     const std::string& key)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);

    fs::path file_path(m_storage_path / collection / key);
    fs::ifstream stream(file_path);
   
    if(!stream) {
        throw storage_error_t("the specified object has not been found");
    }
    
    m_log->debug(
        "reading the '%s' object, collection: '%s', path: '%s'",
        key.c_str(),
        collection.c_str(),
        file_path.string().c_str()
    );

    std::stringstream buffer;
    buffer << stream.rdbuf();

    return buffer.str();
}

void
file_storage_t::write(const std::string& collection,
                      const std::string& key,
                      const std::string& blob) 
{
    boost::lock_guard<boost::mutex> lock(m_mutex);

    fs::path store_path(m_storage_path / collection);

    if(!fs::exists(store_path)) {
        m_log->info(
            "creating collection: %s, path: '%s'",
            collection.c_str(),
            store_path.string().c_str()
        );

        try {
            fs::create_directories(store_path);
        } catch(const fs::filesystem_error& e) {
            throw storage_error_t("cannot create the specified collection");
        }
    } else if(fs::exists(store_path) && !fs::is_directory(store_path)) {
        throw storage_error_t("the specified collection is corrupted");
    }
    
    fs::path file_path(store_path / key);
    
    fs::ofstream stream(
        file_path,
        fs::ofstream::out | fs::ofstream::trunc
    );
   
    if(!stream) {
        throw storage_error_t("unable to access the specified object"); 
    }     

    m_log->debug(
        "writing the '%s' object, collection: '%s', path: '%s'",
        key.c_str(),
        collection.c_str(),
        file_path.string().c_str()
    );

    stream << blob;
    stream.close();
}

namespace {
    struct validate_t {
        template<typename T>
        bool
        operator()(const T& entry) {
            return fs::is_regular(entry);
        }
    };
}

std::vector<std::string>
file_storage_t::list(const std::string& collection) {
    boost::lock_guard<boost::mutex> lock(m_mutex);

    fs::path store_path(m_storage_path / collection);
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
#if BOOST_FILESYSTEM_VERSION == 3
        result.push_back(it->path().filename().string());
#else
        result.push_back(it->leaf());
#endif
 
        ++it;
    }

    return result;
}

void
file_storage_t::remove(const std::string& collection,
                     const std::string& key)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);
    
    fs::path file_path(m_storage_path / collection / key);
    
    if(fs::exists(file_path)) {
        m_log->debug(
            "removing the '%s' object, collection: '%s', path: %s",
            key.c_str(),
            collection.c_str(),
            file_path.string().c_str()
        );

        try {
            fs::remove(file_path);
        } catch(const fs::filesystem_error& e) {
            throw storage_error_t("unable to remove the specified object");
        }
    }
}
