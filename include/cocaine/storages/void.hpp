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

#ifndef COCAINE_VOID_STORAGE_HPP
#define COCAINE_VOID_STORAGE_HPP

#include "cocaine/interfaces/storage.hpp"

namespace cocaine { namespace storages {

template<class StorageType>
class void_storage:
    public StorageType
{
    public:
        typedef StorageType category_type;
        typedef typename category_type::value_type value_type;

    public:
        void_storage(context_t& context, const std::string& uri):
            category_type(context, uri)
        { }

        virtual void put(const std::string& ns,
                         const std::string& key,
                         const value_type& value)
        { }

        virtual bool exists(const std::string& ns,
                            const std::string& key)
        {
            return false;
        }

        virtual value_type get(const std::string& ns,
                               const std::string& key)
        {
            return value_type();
        }

        virtual std::vector<std::string> list(const std::string& ns) {
            return std::vector<std::string>();
        }

        virtual void remove(const std::string& ns,
                            const std::string& key)
        { }
        
        virtual void purge(const std::string& ns) { }
};

typedef void_storage<document_storage_t> document_void_storage_t;
typedef void_storage<blob_storage_t> blob_void_storage_t;

}}

#endif
