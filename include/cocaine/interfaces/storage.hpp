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

#ifndef COCAINE_STORAGE_INTERFACE_HPP
#define COCAINE_STORAGE_INTERFACE_HPP

#include "cocaine/common.hpp"
#include "cocaine/registry.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace storages {

class storage_t:
    public boost::noncopyable
{
    public:
        typedef core::policies::share policy;

    public:
        virtual ~storage_t() = 0;

        virtual void put(const std::string& ns,
                         const std::string& key,
                         const Json::Value& value) = 0;
        
        virtual bool exists(const std::string& ns, const std::string& key) = 0;
        virtual Json::Value get(const std::string& ns, const std::string& key) = 0;
        virtual Json::Value all(const std::string& ns) = 0;

        virtual void remove(const std::string& ns, const std::string& key) = 0;
        virtual void purge(const std::string& ns) = 0;

    protected:
        storage_t(context_t& ctx);

    protected:
        context_t& m_context;
};

}}

#endif
