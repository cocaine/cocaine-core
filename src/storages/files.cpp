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

using namespace cocaine;
using namespace cocaine::storages;

namespace fs = boost::filesystem;

blob_file_storage_t::blob_file_storage_t(context_t& context, const Json::Value& args):
    category_type(context, args),
    m_storage_path(args["path"].asString())
{ }

void blob_file_storage_t::put(const std::string& ns,
                              const std::string& key,
                              const value_type& blob) 
{
    fs::path store_path(m_storage_path / ns);

    if(!fs::exists(store_path)) {
        try {
            fs::create_directories(store_path);
        } catch(const std::runtime_error& e) {
            throw storage_error_t("cannot create " + store_path.string());
        }
    } else if(fs::exists(store_path) && !fs::is_directory(store_path)) {
        throw storage_error_t(store_path.string() + " is not a directory");
    }
    
    fs::path file_path(store_path / key);
    fs::ofstream stream(file_path, fs::ofstream::out | fs::ofstream::trunc);
   
    if(!stream) {
        throw storage_error_t("unable to open " + file_path.string()); 
    }     

    stream.write(
        static_cast<const char*>(blob.data()),
        blob.size()
    );

    stream.close();
}

bool blob_file_storage_t::exists(const std::string& ns,
                                 const std::string& key)
{
    fs::path file_path(m_storage_path / ns / key);
    
    return (fs::exists(file_path) && 
            fs::is_regular(file_path));
}

blob_file_storage_t::value_type blob_file_storage_t::get(const std::string& ns,
                                                         const std::string& key)
{
    fs::path file_path(m_storage_path / ns / key);
    fs::ifstream stream(file_path, fs::ifstream::in);
    
    if(stream) {
        std::stringstream buffer;

        buffer << stream.rdbuf();

        return value_type(
            buffer.str().data(),
            buffer.str().size()
        );
    } else {
        throw storage_error_t("the specified object has not been found");
    }
}

namespace {
    struct is_regular_file {
        template<typename T> bool operator()(const T& entry) {
            return fs::is_regular(entry);
        }
    };
}

std::vector<std::string> blob_file_storage_t::list(const std::string& ns) {
    fs::path store_path(m_storage_path / ns);
    std::vector<std::string> result;

    if(!fs::exists(store_path)) {
        return result;
    }

    typedef boost::filter_iterator<is_regular_file, fs::directory_iterator> file_iterator;
    
    file_iterator it = file_iterator(is_regular_file(), fs::directory_iterator(store_path)), 
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

void blob_file_storage_t::remove(const std::string& ns,
                                 const std::string& key)
{
    fs::path file_path(m_storage_path / ns / key);
    
    if(fs::exists(file_path)) {
        try {
            fs::remove(file_path);
        } catch(const std::runtime_error& e) {
            throw storage_error_t("unable to remove " + file_path.string());
        }
    }
}

void blob_file_storage_t::purge(const std::string& ns) {
    fs::path store_path(m_storage_path / ns);
    fs::remove_all(store_path);
}

void document_file_storage_t::put(const std::string& ns,
                                  const std::string& key,
                                  const value_type& value)
{
    std::string json(Json::StyledWriter().write(value));

    m_storage.put(
        ns,
        key,
        blob_storage_t::value_type(
            json.data(),
            json.size()
        )
    );
}

document_file_storage_t::value_type document_file_storage_t::get(const std::string& ns,
                                                                 const std::string& key)
{
    blob_storage_t::value_type value(m_storage.get(ns, key));
    value_type result;

    if(!value.empty()) {
        Json::Reader reader(Json::Features::strictMode());

        std::string json(
            static_cast<const char*>(value.data()),
            static_cast<const char*>(value.data()) + value.size()
        );
        
        if(!reader.parse(json, result)) {
            throw storage_error_t("unparseable json in the storage");
        }
    }

    return result;
}
