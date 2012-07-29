/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

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
        storage_concept(context_t& context, const std::string& , const Json::Value& ):
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

    typedef boost::tuple<
        const std::string&,
        const Json::Value&
    > args_type;
    
    template<class U>
    struct default_factory:
        public factory<storage_type>
    {
        virtual ptr_type get(context_t& context,
                             const args_type& args)
        {
            boost::lock_guard<boost::mutex> lock(m_mutex);

            const std::string& name(boost::get<0>(args));

            typename instance_map_t::iterator it(
                m_instances.find(name)
            );

            if(it == m_instances.end()) {
                boost::tie(it, boost::tuples::ignore) = m_instances.insert(
                    std::make_pair(
                        name,
                        boost::make_shared<U>(
                            boost::ref(context),
                            name,
                            boost::get<1>(args)
                        )
                    )
                );
            }

            return it->second;
        }

    private:
        typedef std::map<
            std::string,
            ptr_type
        > instance_map_t;

        instance_map_t m_instances;
        boost::mutex m_mutex;
    };
};

}

#endif
