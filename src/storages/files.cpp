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
#include <boost/iterator/filter_iterator.hpp>
#include <msgpack.hpp>

#include "cocaine/storages/files.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::storages;

namespace fs = boost::filesystem;

namespace msgpack {
    template<class Stream>
    inline packer<Stream>& operator << (packer<Stream>& packer, 
                                        const objects::value_type& object)
    {
        Json::FastWriter writer;
        
        packer.pack_array(2);

        packer << writer.write(object.meta);

        packer.pack_raw(object.blob.size());

        packer.pack_raw_body(
            static_cast<const char*>(object.blob.data()),
            object.blob.size()
        );

        return packer;
    }

    inline objects::value_type& operator >> (const msgpack::object& o,
                                             objects::value_type& object)
    {
        Json::Reader reader(Json::Features::strictMode());

        if(o.type != type::ARRAY || o.via.array.size != 2) {
            throw type_error();
        }

        msgpack::object &meta = o.via.array.ptr[0],
                        &blob = o.via.array.ptr[1];

        std::string json(meta.as<std::string>());

        if(!reader.parse(json, object.meta)) {
            throw type_error();
        }

        object.blob = objects::data_type(
            blob.via.raw.ptr,
            blob.via.raw.size
        );

        return object;
    }
}

file_storage_t::file_storage_t(context_t& context, const plugin_config_t& config):
    category_type(context, config),
    m_log(context.log("storage/" + config.name)),
    m_storage_path(config.args["path"].asString())
{ }

objects::value_type file_storage_t::get(const std::string& ns,
                                        const std::string& key)
{
    boost::lock_guard<boost::mutex> lock(m_mutex);
    fs::path file_path(m_storage_path / ns / key);
    fs::ifstream stream(file_path);
   
    m_log->debug(
        "reading the '%s' object, namespace: '%s', path: '%s'",
        key.c_str(),
        ns.c_str(),
        file_path.string().c_str()
    );

    if(!stream) {
        throw storage_error_t("the specified object has not been found");
    }
    
    std::stringstream buffer;
    msgpack::unpacked unpacked;

    buffer << stream.rdbuf();
    std::string blob(buffer.str());
    
    try {
        msgpack::unpack(&unpacked, blob.data(), blob.size());
        return unpacked.get().as<objects::value_type>();
    } catch (const std::exception& e) {
        m_log->error(
            "the '%s' object is corrupted, namespace: '%s', path: '%s'",
            key.c_str(),
            ns.c_str(),
            file_path.string().c_str()
        );

        throw storage_error_t("the specified object is corrupted");
    }
}

void file_storage_t::put(const std::string& ns,
                         const std::string& key,
                         const objects::value_type& object) 
{
    boost::lock_guard<boost::mutex> lock(m_mutex);
    fs::path store_path(m_storage_path / ns);

    if(!fs::exists(store_path)) {
        m_log->info(
            "creating the '%s' namespace, path: '%s'",
            ns.c_str(),
            store_path.string().c_str()
        );

        try {
            fs::create_directories(store_path);
        } catch(const std::runtime_error& e) {
            throw storage_error_t("cannot create the specified namespace");
        }
    } else if(fs::exists(store_path) && !fs::is_directory(store_path)) {
        throw storage_error_t("the specified namespace is corrupted");
    }
    
    fs::path file_path(store_path / key);
    
    fs::ofstream stream(
        file_path,
        fs::ofstream::out | fs::ofstream::trunc
    );
   
    if(!stream) {
        throw storage_error_t("unable to access the specified object"); 
    }     

    msgpack::packer<fs::ofstream> packer(stream);

    m_log->debug(
        "writing the '%s' object, namespace '%s', path: '%s'",
        key.c_str(),
        ns.c_str(),
        file_path.string().c_str()
    );

    packer << object;

    stream.close();
}

objects::meta_type file_storage_t::exists(const std::string& ns,
                                          const std::string& key)
{
    return get(ns, key).meta;
}

namespace {
    struct is_regular_file {
        template<typename T> bool operator()(const T& entry) {
            return fs::is_regular(entry);
        }
    };
}

std::vector<std::string> file_storage_t::list(const std::string& ns) {
    boost::lock_guard<boost::mutex> lock(m_mutex);
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
    boost::lock_guard<boost::mutex> lock(m_mutex);
    fs::path file_path(m_storage_path / ns / key);
    
    if(fs::exists(file_path)) {
        m_log->debug(
            "removing the '%s' object, namespace: '%s', path: %s",
            key.c_str(),
            ns.c_str(),
            file_path.string().c_str()
        );

        try {
            fs::remove(file_path);
        } catch(const std::runtime_error& e) {
            throw storage_error_t("unable to remove the specified object");
        }
    }
}
