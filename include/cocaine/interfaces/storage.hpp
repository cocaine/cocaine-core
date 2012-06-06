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
#include "cocaine/helpers/blob.hpp"

namespace cocaine { namespace storages {

struct document {
    typedef Json::Value value_type;
};

struct blob {
    typedef blob_t value_type;
};

template<class T>
class storage_concept:
    public boost::noncopyable
{
    public:
        typedef typename T::value_type value_type;

    public:
        virtual ~storage_concept() { 
            // Empty.
        }

        virtual void put(const std::string& ns,
                         const std::string& key,
                         const value_type& value) = 0;
        
        virtual bool exists(const std::string& ns,
                            const std::string& key) = 0;

        virtual value_type get(const std::string& ns,
                               const std::string& key) = 0;

        virtual std::vector<std::string> list(const std::string& ns) = 0;

        virtual void remove(const std::string& ns,
                            const std::string& key) = 0;
        
        virtual void purge(const std::string& ns) = 0;

    protected:
        storage_concept(context_t& context, const Json::Value& args):
            m_context(context)
        { }

    protected:
        context_t& m_context;
};

typedef storage_concept<document> document_storage_t;
typedef storage_concept<blob> blob_storage_t;

}

template<class T> struct category_traits< storages::storage_concept<T> > {
    typedef storages::storage_concept<T> storage_type;
    typedef boost::shared_ptr<storage_type> ptr_type;
    typedef boost::tuple<const Json::Value&> args_type;
    
    template<class U>
    struct default_factory:
        public factory<storage_type>
    {
        virtual ptr_type get(context_t& context,
                             const args_type& args)
        {
            boost::lock_guard<boost::mutex> lock(m_mutex);
            const Json::Value& json(boost::get<0>(args));

            typename storage_map_t::iterator it(
                m_storages.find(json)
            );

            if(it == m_storages.end()) {
                boost::tie(it, boost::tuples::ignore) = m_storages.insert(
                    std::make_pair(
                        json,
                        boost::make_shared<U>(
                            boost::ref(context),
                            json
                        )
                    )
                );
            }

            return it->second;
        }

    private:
        boost::mutex m_mutex;

        typedef std::map<
            Json::Value,
            ptr_type
        > storage_map_t;

        storage_map_t m_storages;
    };
};

}

#endif
