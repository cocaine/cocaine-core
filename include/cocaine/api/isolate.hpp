/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_ISOLATE_API_HPP
#define COCAINE_ISOLATE_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "json/json.h"

#include <mutex>

namespace cocaine { namespace api {

struct handle_t {
    virtual
   ~handle_t() {
        // Empty.
    }

    virtual
    void
    terminate() = 0;
};

class isolate_t {
    public:
        typedef isolate_t category_type;

    public:
        virtual
       ~isolate_t() {
            // Empty.
        }

        virtual
        std::unique_ptr<handle_t>
        spawn(const std::string& path,
              const std::map<std::string, std::string>& args,
              const std::map<std::string, std::string>& environment,
              int pipe) = 0;

    protected:
        isolate_t(context_t&,
                  const std::string& /* name */,
                  const Json::Value& /* args */)
        { }
};

template<>
struct category_traits<isolate_t> {
    typedef std::shared_ptr<isolate_t> ptr_type;

    struct factory_type:
        public basic_factory<isolate_t>
    {
        virtual
        ptr_type
        get(context_t& context,
            const std::string& name,
            const Json::Value& args) = 0;
    };

    template<class T>
    struct default_factory:
        public factory_type
    {
        virtual
        ptr_type
        get(context_t& context,
            const std::string& name,
            const Json::Value& args)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            typename instance_map_t::iterator it(m_instances.find(name));

            ptr_type instance;

            if(it != m_instances.end()) {
                instance = it->second.lock();
            }

            if(!instance) {
                instance = std::make_shared<T>(
                    std::ref(context),
                    name,
                    args
                );

                m_instances[name] = instance;
            }

            return instance;
        }

    private:
        typedef std::map<
            std::string,
            std::weak_ptr<isolate_t>
        > instance_map_t;

        instance_map_t m_instances;
        std::mutex m_mutex;
    };
};

}} // namespace cocaine::api

#endif
