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
#include <msgpack.hpp>

#include "cocaine/storages/files.hpp"

using namespace cocaine;
using namespace cocaine::storages;

namespace fs = boost::filesystem;

namespace msgpack {
    template<class Stream>
    inline packer<Stream>& operator << (packer<Stream>& packer, const objects::value_type& value) {
        std::string json(Json::FastWriter().write(value.meta));

        packer.pack_array(2);

        packer.pack_raw(json.size());
        packer.pack_raw_body(json.data(), json.size());

        packer.pack_raw(value.blob.size());

        packer.pack_raw_body(
            static_cast<const char*>(value.blob.data()),
            value.blob.size()
        );

        return packer;
    }

    inline objects::value_type& operator >> (msgpack::object o, objects::value_type& value) {
        if(o.type != type::ARRAY || o.via.array.size != 2) {
            throw type_error();
        }

        Json::Reader reader(Json::Features::strictMode());

        msgpack::object &meta = o.via.array.ptr[0],
                        &blob = o.via.array.ptr[1];

        if(!reader.parse(
            meta.via.raw.ptr,
            meta.via.raw.ptr + meta.via.raw.size,
            value.meta))
        {
            throw type_error();
        }

        value.blob = objects::data_type(
            blob.via.raw.ptr,
            blob.via.raw.size
        );

        return value;
    }
}

file_storage_t::file_storage_t(context_t& context, const Json::Value& args):
    category_type(context, args),
    m_storage_path(args["path"].asString())
{ }

void file_storage_t::put(const std::string& ns,
                         const std::string& key,
                         const value_type& value) 
{
    fs::path store_path(m_storage_path / ns);

    if(!fs::exists(store_path)) {
        try {
            fs::create_directories(store_path);
        } catch(const std::runtime_error& e) {
            throw storage_error_t("cannot create the specified container");
        }
    } else if(fs::exists(store_path) && !fs::is_directory(store_path)) {
        throw storage_error_t("the specified container is corrupted");
    }
    
    fs::path file_path(store_path / key);
    fs::ofstream stream(file_path, fs::ofstream::out | fs::ofstream::trunc);
   
    if(!stream) {
        throw storage_error_t("unable to open the specified object"); 
    }     

    msgpack::sbuffer buffer;
    msgpack::pack(buffer, value);

    stream.write(
        static_cast<const char*>(buffer.data()),
        buffer.size()
    );

    stream.close();
}

objects::meta_type file_storage_t::exists(const std::string& ns,
                                          const std::string& key)
{
    return get(ns, key).meta;
}

file_storage_t::value_type file_storage_t::get(const std::string& ns,
                                               const std::string& key)
{
    fs::path file_path(m_storage_path / ns / "data" / key);
    fs::ifstream stream(file_path, fs::ifstream::in);
    
    if(stream) {
        std::stringstream buffer;
        msgpack::unpacked unpacked;
        objects::value_type value;

        buffer << stream.rdbuf();

        try {
            msgpack::unpack(&unpacked, buffer.str().data(), buffer.str().size());
        } catch (const msgpack::type_error& e) {
            throw storage_error_t("the specified object is corrupted");
        }

        unpacked.get().convert(&value);

        return value;
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

std::vector<std::string> file_storage_t::list(const std::string& ns) {
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

void file_storage_t::remove(const std::string& ns,
                            const std::string& key)
{
    fs::path file_path(m_storage_path / ns / key);
    
    if(fs::exists(file_path)) {
        try {
            fs::remove(file_path);
        } catch(const std::runtime_error& e) {
            throw storage_error_t("unable to remove the specified object");
        }
    }
}
