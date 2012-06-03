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

#include <boost/thread/mutex.hpp>
#include <boost/tuple/tuple.hpp>

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace storages {

class storage_t:
    public boost::noncopyable
{
    public:
        virtual ~storage_t() = 0;

        virtual void put(const std::string& ns,
                         const std::string& key,
                         const Json::Value& value) = 0;
        
        virtual bool exists(const std::string& ns,
                            const std::string& key) = 0;

        virtual Json::Value get(const std::string& ns,
                                const std::string& key) = 0;

        virtual Json::Value all(const std::string& ns) = 0;

        virtual void remove(const std::string& ns,
                            const std::string& key) = 0;
        
        virtual void purge(const std::string& ns) = 0;

    protected:
        storage_t(context_t& context,
                  const std::string& uri);

    protected:
        context_t& m_context;
};

}

template<> struct category_traits<storages::storage_t> {
    typedef boost::shared_ptr<storages::storage_t> ptr_type;
    typedef boost::tuple<const std::string&> args_type;
    
    template<class T>
    struct factory_type:
        public category_model<storages::storage_t>
    {
        virtual ptr_type get(context_t& context,
                             const args_type& args)
        {
            boost::lock_guard<boost::mutex> lock(m_mutex);
            const std::string& uri(boost::get<0>(args));

            storage_map_t::iterator it(
                m_storages.find(uri)
            );

            if(it == m_storages.end()) {
                boost::tie(it, boost::tuples::ignore) = m_storages.insert(
                    std::make_pair(
                        uri,
                        boost::make_shared<T>(
                            boost::ref(context),
                            uri
                        )
                    )
                );
            }

            return it->second;
        }

    private:
        boost::mutex m_mutex;

        typedef std::map<
            std::string,
            ptr_type
        > storage_map_t;

        storage_map_t m_storages;
    };
};

}

#endif
