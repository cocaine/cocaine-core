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

#ifndef COCAINE_ELLIPTICS_STORAGE_HPP
#define COCAINE_ELLIPTICS_STORAGE_HPP

#include <elliptics/cppdef.h>

#include "cocaine/interfaces/storage.hpp"

namespace cocaine { namespace storages {

class log_adapter_t:
    public zbr::elliptics_log
{
    public:
        log_adapter_t(context_t& context,
                      const uint32_t mask);

        virtual void log(const uint32_t mask, const char * message);
        virtual unsigned long clone();

    private:
        context_t& m_context;
        const uint32_t m_mask;

        boost::shared_ptr<logging::logger_t> m_log;
};

class elliptics_storage_t:
    public storage_concept<objects>
{
    public:
        typedef storage_concept<objects> category_type;

    public:
        elliptics_storage_t(context_t& context,
                            const Json::Value& args);

        virtual objects::value_type get(const std::string& ns,
                                        const std::string& key);

        virtual void put(const std::string& ns,
                         const std::string& key,
                         const objects::value_type& object);

        virtual objects::meta_type exists(const std::string& ns,
                                          const std::string& key);

        virtual std::vector<std::string> list(const std::string& ns);

        virtual void remove(const std::string& ns,
                            const std::string& key);

    private:
        std::string id(const std::string& ns,
                       const std::string& key)
        {
            return ns + '\0' + key;
        };

    private:
        boost::shared_ptr<logging::logger_t> m_log;
        
        log_adapter_t m_log_adapter;
        zbr::elliptics_node m_node;
};

}}

#endif
