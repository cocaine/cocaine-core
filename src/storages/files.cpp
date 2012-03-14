//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <boost/filesystem/fstream.hpp>
#include <boost/iterator/filter_iterator.hpp>

#include "cocaine/storages/files.hpp"

#include "cocaine/context.hpp"

using namespace cocaine::storages;

namespace fs = boost::filesystem;

struct is_regular_file {
    template<typename T> bool operator()(T entry) {
        return fs::is_regular(entry);
    }
};

file_storage_t::file_storage_t(context_t& context):
    storage_t(context),
    m_storage_path(context.config.storage.uri)
{ }

void file_storage_t::put(const std::string& ns,
                         const std::string& key,
                         const Json::Value& value) 
{
    fs::path store_path(m_storage_path / ns);

    if(!fs::exists(store_path)) {
        try {
            fs::create_directories(store_path);
        } catch(const std::runtime_error& e) {
            throw std::runtime_error("cannot create " + store_path.string());
        }
    } else if(fs::exists(store_path) && !fs::is_directory(store_path)) {
        throw std::runtime_error(store_path.string() + " is not a directory");
    }
    
    fs::path file_path(store_path / key);
    fs::ofstream stream(file_path, fs::ofstream::out | fs::ofstream::trunc);
   
    if(!stream) {
        throw std::runtime_error("unable to open " + file_path.string()); 
    }     

    std::string json(Json::StyledWriter().write(value));
    
    stream << json;
    stream.close();
}

bool file_storage_t::exists(const std::string& ns, const std::string& key) {
    fs::path file_path(m_storage_path / ns / key);
    
    return (fs::exists(file_path) && 
            fs::is_regular(file_path));
}

Json::Value file_storage_t::get(const std::string& ns, const std::string& key) {
    Json::Value root(Json::objectValue);
    fs::path file_path(m_storage_path / ns / key);
    fs::ifstream stream(file_path, fs::ifstream::in);
    
    if(stream) { 
        Json::Reader reader(Json::Features::strictMode());
        
        if(!reader.parse(stream, root)) {
            throw std::runtime_error("corrupted data in " + file_path.string());
        }
    }

    return root;
}

Json::Value file_storage_t::all(const std::string& ns) {
    Json::Value root(Json::objectValue);
    fs::path store_path(m_storage_path / ns);

    if(!fs::exists(store_path)) {
        return root;
    }

    typedef boost::filter_iterator<is_regular_file, fs::directory_iterator> file_iterator;
    file_iterator it = file_iterator(is_regular_file(), fs::directory_iterator(store_path)), end;

    while(it != end) {
#if BOOST_FILESYSTEM_VERSION == 3
        Json::Value value(get(ns, it->path().filename().string()));
#else
        Json::Value value(get(ns, it->leaf()));
#endif

        if(!value.empty()) {
#if BOOST_FILESYSTEM_VERSION == 3
            root[it->path().filename().string()] = value;
#else
            root[it->leaf()] = value;
#endif
        }

        ++it;
    }

    return root;
}

void file_storage_t::remove(const std::string& ns, const std::string& key) {
    fs::path file_path(m_storage_path / ns / key);
    
    if(fs::exists(file_path)) {
        try {
            fs::remove(file_path);
        } catch(...) {
            throw std::runtime_error("unable to remove " + file_path.string());
        }
    }
}

void file_storage_t::purge(const std::string& ns) {
    fs::path store_path(m_storage_path / ns);
    fs::remove_all(store_path);
}

