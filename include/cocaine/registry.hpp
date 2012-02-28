//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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

#ifndef COCAINE_REGISTRY_HPP
#define COCAINE_REGISTRY_HPP

#include <ltdl.h>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/object.hpp"

#include "cocaine/interfaces/module.hpp"

namespace cocaine { namespace core {

// Module registry
// ---------------

class registry_t:
    public object_t
{
    public:
        registry_t(context_t& ctx);
        ~registry_t();

        bool exists(const std::string& type);

        template<class T>
        std::auto_ptr<T> create(const std::string& type) {
            factory_map_t::iterator it(m_factories.find(type));

            if(it == m_factories.end()) {
                throw std::runtime_error("module '" + type + "' is not available");
            }

            object_t* object = it->second(context());
            T* module = dynamic_cast<T*>(object);

            if(module) {
                return std::auto_ptr<T>(module);
            } else {
                delete object;
                throw std::runtime_error("module '" + type + "' has a wrong type");
            }
        }

    private:
        // Used to unload all the modules on shutdown
        std::vector<lt_dlhandle> m_modules;
    
        // Used to instantiate the modules
        typedef std::map<const std::string, factory_fn_t> factory_map_t;
        factory_map_t m_factories;
};

}}

#endif
