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
        bool callback(const zbr::eblob_disk_control * dco, const void * data, int);
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
        eblob_purger_t(zbr::eblob * eblob);

        bool callback(const zbr::eblob_disk_control * dco, const void * data, int);
        void complete(uint64_t, uint64_t);

    private:
        zbr::eblob * m_eblob;

        typedef std::vector<zbr::eblob_key> key_list_t;
        key_list_t m_keys;
};

class eblob_storage_t:
    public storage_t
{
    public:
        eblob_storage_t(context_t& context);
        ~eblob_storage_t();

        virtual void put(const std::string& ns,
                         const std::string& key,
                         const Json::Value& value);

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
