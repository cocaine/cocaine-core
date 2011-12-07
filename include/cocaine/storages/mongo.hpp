//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_STORAGE_MONGO_HPP
#define COCAINE_STORAGE_MONGO_HPP

#include <mongo/client/dbclient.h>

#include "cocaine/storages/base.hpp"

namespace cocaine { namespace storage {

class mongo_storage_t:
    public storage_t
{
    public:
        mongo_storage_t();

        virtual void put(const std::string& ns, const std::string& key, const Json::Value& value);

        virtual bool exists(const std::string& ns, const std::string& key);
        virtual Json::Value get(const std::string& ns, const std::string& key);
        virtual Json::Value all(const std::string& ns);

        virtual void remove(const std::string& ns, const std::string& key);
        virtual void purge(const std::string& ns);

    private:
        inline std::string resolve(const std::string& ns) {
            return "cocaine." + m_instance + "." + ns;
        }

    private:
        const std::string m_instance;
        const mongo::ConnectionString m_url;
};

}}

#endif
