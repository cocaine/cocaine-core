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

#ifndef COCAINE_STORAGE_API_HPP
#define COCAINE_STORAGE_API_HPP

#include <boost/thread/mutex.hpp>
#include <boost/tuple/tuple.hpp>

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"
#include "cocaine/traits.hpp"

namespace cocaine { namespace api {

class storage_t:
    public boost::noncopyable
{
    public:
        virtual
        ~storage_t() { 
            // Empty.
        }

        template<class T>
        T
        get(const std::string& collection,
            const std::string& key)
        {
            T result;
            msgpack::unpacked unpacked;
            
            std::string blob(
                read(collection, key)
            );

            try {
                msgpack::unpack(&unpacked, blob.data(), blob.size());
                io::type_traits<T>::unpack(unpacked.get(), result);
            } catch(const std::exception& e) {
                throw storage_error_t("corrupted object");
            }
            
            return result;
        }

        template<class T>
        void
        put(const std::string& collection,
            const std::string& key,
            const T& object)
        {
            msgpack::sbuffer buffer;
            msgpack::packer<msgpack::sbuffer> packer(buffer);
        
            try {
                io::type_traits<T>::pack(packer, object);
            } catch(const std::exception& e) {
                throw storage_error_t("corrupted object");
            }

            std::string blob(
                buffer.data(),
                buffer.size()
            );
            
            write(collection, key, blob);
        }

        virtual
        std::vector<std::string>
        list(const std::string& collection) = 0;

        virtual
        void
        remove(const std::string& collection,
               const std::string& key) = 0;
    
    protected:
        storage_t(context_t& context, const std::string&, const Json::Value&):
            m_context(context)
        { }

        virtual
        std::string
        read(const std::string& collection,
             const std::string& key) = 0;

        virtual
        void
        write(const std::string& collection,
              const std::string& key,
              const std::string& blob) = 0;

    private:
        context_t& m_context;
};

template<>
struct category_traits<storage_t> {
    typedef boost::shared_ptr<storage_t> ptr_type;

    typedef boost::tuple<
        const std::string&,
        const Json::Value&
    > args_type;
    
    template<class T>
    struct default_factory:
        public factory<storage_t>
    {
        virtual
        ptr_type
        get(context_t& context,
            const args_type& args)
        {
            boost::lock_guard<boost::mutex> lock(m_mutex);

            const std::string& name(
                boost::get<0>(args)
            );

            typename instance_map_t::iterator it(
                m_instances.find(name)
            );

            if(it == m_instances.end()) {
                boost::tie(it, boost::tuples::ignore) = m_instances.insert(
                    std::make_pair(
                        name,
                        boost::make_shared<T>(
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

}} // namespace cocaine::api

#endif
