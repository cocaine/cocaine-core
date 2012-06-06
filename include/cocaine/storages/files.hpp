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

#ifndef COCAINE_FILE_STORAGE_HPP
#define COCAINE_FILE_STORAGE_HPP

#include <boost/filesystem.hpp>

#include "cocaine/interfaces/storage.hpp"

namespace cocaine { namespace storages {

class blob_file_storage_t:
    public blob_storage_t
{
    public:
        typedef blob_storage_t category_type;
        typedef category_type::value_type value_type;

    public:
        blob_file_storage_t(context_t& context,
                            const Json::Value& args);

        virtual void put(const std::string& ns, 
                         const std::string& key, 
                         const value_type& value);

        virtual bool exists(const std::string& ns,
                            const std::string& key);

        virtual value_type get(const std::string& ns,
                               const std::string& key);

        virtual std::vector<std::string> list(const std::string& ns);

        virtual void remove(const std::string& ns,
                            const std::string& key);
        
        virtual void purge(const std::string& ns);

    private:
        const boost::filesystem::path m_storage_path;
};

class document_file_storage_t:
    public document_storage_t
{
    public:
        typedef document_storage_t category_type;
        typedef category_type::value_type value_type;

    public:
        document_file_storage_t(context_t& context, const Json::Value& args):
            category_type(context, args),
            m_storage(context, args)
        { }

        virtual void put(const std::string& ns, 
                         const std::string& key, 
                         const value_type& value);

        virtual bool exists(const std::string& ns,
                            const std::string& key)
        {
            return m_storage.exists(ns, key);
        }

        virtual value_type get(const std::string& ns,
                               const std::string& key);

        virtual std::vector<std::string> list(const std::string& ns) {
            return m_storage.list(ns);
        }

        virtual void remove(const std::string& ns,
                            const std::string& key)
        {
            m_storage.remove(ns, key);
        }
        
        virtual void purge(const std::string& ns) {
            m_storage.purge(ns);
        }

    private:
        blob_file_storage_t m_storage;
};

}}

#endif
