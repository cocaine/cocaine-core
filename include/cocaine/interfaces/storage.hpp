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

template<class T>
class concept:
    public boost::noncopyable
{
    public:
        virtual ~concept() { 
            // Empty.
        }

        virtual void put(const std::string& ns,
                         const std::string& key,
                         const T& value) = 0;
        
        virtual bool exists(const std::string& ns,
                            const std::string& key) = 0;

        virtual T get(const std::string& ns,
                                const std::string& key) = 0;

        virtual T all(const std::string& ns) = 0;

        virtual void remove(const std::string& ns,
                            const std::string& key) = 0;
        
        virtual void purge(const std::string& ns) = 0;

    protected:
        concept(context_t& context, const std::string& uri):
            m_context(context)
        { }

    protected:
        context_t& m_context;
};

typedef concept<json_t> json_storage_t;
typedef concept<blob_t> blob_storage_t;

}

template<class DataType> struct category_traits< storages::concept<DataType> > {
    typedef storages::concept<DataType> storage_type;
    typedef boost::shared_ptr<storage_type> ptr_type;
    typedef boost::tuple<const std::string&> args_type;
    
    template<class T>
    struct factory_type:
        public category_model<storage_type>
    {
        virtual ptr_type get(context_t& context,
                             const args_type& args)
        {
            boost::lock_guard<boost::mutex> lock(m_mutex);
            const std::string& uri(boost::get<0>(args));

            typename storage_map_t::iterator it(
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
