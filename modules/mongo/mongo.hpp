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
    public storage_t
{
    public:
        mongo_storage_t(context_t& ctx);

        virtual void put(const std::string& ns,
                         const std::string& key,
                         const Json::Value& value);

        virtual bool exists(const std::string& ns, const std::string& key);
        virtual Json::Value get(const std::string& ns, const std::string& key);
        virtual Json::Value all(const std::string& ns);

        virtual void remove(const std::string& ns, const std::string& key);
        virtual void purge(const std::string& ns);

    private:
        std::string resolve(const std::string& ns) const;

    private:
        const mongo::ConnectionString m_uri;
};

}}

#endif
