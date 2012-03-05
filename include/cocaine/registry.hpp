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

#include <typeinfo>
#include <boost/type_traits/is_base_of.hpp>

#include <ltdl.h>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/object.hpp"

namespace cocaine { namespace core {

// Type-erased factory
// -------------------

class factory_concept_t {
    public:
        virtual const std::type_info& category() const = 0;
        virtual object_t* create(context_t& ctx) = 0;
};

template<class T, class Category>
class factory_model_t:
    public factory_concept_t
{
    public:
        virtual const std::type_info& category() const {
            return typeid(Category);
        }

        virtual object_t* create(context_t& ctx) {
            return new T(ctx);
        }
};

// Module registry
// ---------------

class registry_t:
    public object_t
{
    public:
        registry_t(context_t& ctx);
        ~registry_t();

        template<class Category>
        std::auto_ptr<Category> create(const std::string& type) {
            factory_map_t::iterator it(m_factories.find(type));

            if(it == m_factories.end()) {
                throw std::runtime_error("module '" + type + "' is not available");
            }

            if(it->second->category() != typeid(Category)) {
                throw std::runtime_error("module '" + type + "' has an incompatible type");
            }

            std::auto_ptr<Category> module(
                dynamic_cast<Category*>(
                    it->second->create(context())
                )
            );

            return module;
        }

        template<class T, class Category>
        typename boost::enable_if<
            boost::is_base_of<Category, T>
        >::type 
        install(const std::string& type) {
            if(m_factories.find(type) != m_factories.end()) {
                throw std::runtime_error("module '" + type + "' has been already installed");
            }

            log().info("registering '%s' module", type.c_str());

            m_factories.insert(
                type,
                new factory_model_t<T, Category>()
            );
        }

    private:
        // Used to unload all the modules on shutdown
        std::vector<lt_dlhandle> m_modules;
    
        // Used to instantiate the modules
#if BOOST_VERSION >= 104000
        typedef boost::ptr_unordered_map<
#else
        typedef boost::ptr_map<
#endif
            const std::string, 
            factory_concept_t
        > factory_map_t;
        
        factory_map_t m_factories;
};

}}

#endif
