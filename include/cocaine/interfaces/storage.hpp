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

#include "cocaine/helpers/blob.hpp"
#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace storages {

struct objects {
    typedef Json::Value meta_type;
    typedef blob_t data_type;

    typedef struct {
        meta_type meta;
        data_type blob;
    } value_type;
};

template<class T>
class storage_concept;

template<>
class storage_concept<objects>:
    public boost::noncopyable
{
    public:
        virtual ~storage_concept() { 
            // Empty.
        }

        virtual objects::value_type get(const std::string& ns,
                                        const std::string& key) = 0;

        virtual void put(const std::string& ns,
                         const std::string& key,
                         const objects::value_type& value) = 0;
        
        virtual objects::meta_type exists(const std::string& ns,
                                          const std::string& key) = 0;

        virtual std::vector<std::string> list(const std::string& ns) = 0;

        virtual void remove(const std::string& ns,
                            const std::string& key) = 0;

    protected:
        storage_concept(context_t& context, const plugin_config_t& config):
            m_context(context)
        { }

    private:
        context_t& m_context;
};

}

template<class T>
struct category_traits< storages::storage_concept<T> > {
    typedef storages::storage_concept<T> storage_type;
    typedef boost::shared_ptr<storage_type> ptr_type;
    typedef boost::tuple<const plugin_config_t&> args_type;
    
    template<class U>
    struct default_factory:
        public factory<storage_type>
    {
        virtual ptr_type get(context_t& context,
                             const args_type& args)
        {
            boost::lock_guard<boost::mutex> lock(m_mutex);
            const plugin_config_t& config(boost::get<0>(args));

            typename storage_map_t::iterator it(
                m_storages.find(config.name)
            );

            if(it == m_storages.end()) {
                boost::tie(it, boost::tuples::ignore) = m_storages.insert(
                    std::make_pair(
                        config.name,
                        boost::make_shared<U>(
                            boost::ref(context),
                            config
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
