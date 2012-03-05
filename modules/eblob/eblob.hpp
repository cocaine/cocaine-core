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

#ifndef COCAINE_EBLOB_STORAGE_HPP
#define COCAINE_EBLOB_STORAGE_HPP

#include <boost/filesystem.hpp>
#include <eblob/eblob.hpp>

#include "cocaine/interfaces/storage.hpp"

namespace cocaine { namespace storages {

class eblob_collector_t:
    public zbr::eblob_iterator_callback
{
    public:
        bool callback(const zbr::eblob_disk_control* dco, const void* data, int);
        void complete(uint64_t, uint64_t);

    public:
        inline Json::Value seal() { 
            return m_root;
        }

    private:
        Json::Reader m_reader;
        Json::Value m_root;
};

class eblob_purger_t:
    public zbr::eblob_iterator_callback
{
    public:
        eblob_purger_t(zbr::eblob* eblob);

        bool callback(const zbr::eblob_disk_control* dco, const void* data, int);
        void complete(uint64_t, uint64_t);

    private:
        zbr::eblob* m_eblob;

        typedef std::vector<zbr::eblob_key> key_list_t;
        key_list_t m_keys;
};

class eblob_storage_t:
    public storage_t
{
    public:
        eblob_storage_t(context_t& ctx);
        ~eblob_storage_t();

        virtual void put(const std::string& ns, const std::string& key, const Json::Value& value);

        virtual bool exists(const std::string& ns, const std::string& key);
        virtual Json::Value get(const std::string& ns, const std::string& key);
        virtual Json::Value all(const std::string& ns);

        virtual void remove(const std::string& ns, const std::string& key);
        virtual void purge(const std::string& ns);

    private:
        const boost::filesystem::path m_storage_path;

        typedef boost::ptr_unordered_map<
            const std::string, 
            zbr::eblob
        > eblob_map_t;
        
        eblob_map_t m_eblobs;

        zbr::eblob_logger m_logger;
};

}}

#endif
