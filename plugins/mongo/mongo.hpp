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

#ifndef COCAINE_MONGO_STORAGE_HPP
#define COCAINE_MONGO_STORAGE_HPP

#include <mongo/client/dbclient.h>

#include "cocaine/interfaces/storage.hpp"

namespace cocaine { namespace storages {

class mongo_storage_t:
    public document_storage_t
{
    public:
        typedef document_storage_t category_type;
        typedef category_type::value_type value_type;

    public:
        mongo_storage_t(context_t& context,
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
        std::string resolve(const std::string& ns) const {
            return "cocaine." + ns;
        }

    private:
        const mongo::ConnectionString m_uri;
};

}}

#endif
