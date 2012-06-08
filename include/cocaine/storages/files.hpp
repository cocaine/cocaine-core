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

class file_storage_t:
    public storage_concept<objects>
{
    public:
        typedef storage_concept<objects> category_type;

    public:
        file_storage_t(context_t& context,
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
        boost::mutex m_mutex;
        boost::shared_ptr<logging::logger_t> m_log;
        
        const boost::filesystem::path m_storage_path;
};

}}

#endif
