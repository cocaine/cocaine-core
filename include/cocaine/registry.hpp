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

#ifndef COCAINE_REGISTRY_HPP
#define COCAINE_REGISTRY_HPP

#include <typeinfo>
#include <boost/thread/mutex.hpp>
#include <boost/type_traits/is_base_of.hpp>

#include <ltdl.h>

#include "cocaine/common.hpp"

namespace cocaine { namespace core {

// Retention policies
// ------------------

namespace policies {
    struct none {
        template<class T>
        struct pointer {
            typedef std::auto_ptr<T> type;
        };
    };

    struct share {
        template<class T>
        struct pointer {
            typedef boost::shared_ptr<T> type;
        };
    };
}

// Class factory
// -------------

class category_concept_t {
    public:
        virtual const std::type_info& category() const = 0;
};

template<class Category>
class factory_concept_t:
    public category_concept_t
{
    typedef typename Category::policy::template pointer<Category>::type pointer_type;
    
    public:
        virtual pointer_type create(context_t& ctx) = 0;
};

template<class T, class Category, class RetentionPolicy>
class factory_t;

template<class T, class Category>
class factory_t<T, Category, policies::none>:
    public factory_concept_t<Category>
{
    typedef typename Category::policy::template pointer<Category>::type pointer_type;
    
    public:
        virtual const std::type_info& category() const {
            return typeid(Category);
        }

        virtual pointer_type create(context_t& ctx) {
            return pointer_type(new T(ctx));
        }
};

template<class T, class Category>
class factory_t<T, Category, policies::share>:
    public factory_concept_t<Category>
{
    typedef typename Category::policy::template pointer<Category>::type pointer_type;
    
    public:
        virtual const std::type_info& category() const {
            return typeid(Category);
        }

        virtual pointer_type create(context_t& ctx) {
            boost::lock_guard<boost::mutex> lock(m_mutex);

            if(!m_instance) {
                m_instance.reset(new T(ctx));
            }

            return m_instance;
        }

    private:
        boost::mutex m_mutex;
        pointer_type m_instance;
};

// Module registry
// ---------------

class registry_t:
    public boost::noncopyable
{
    public:
        registry_t(context_t& ctx);
        ~registry_t();

        template<class Category>
        typename Category::policy::template pointer<Category>::type
        create(const std::string& type) {
            factory_map_t::iterator it(m_factories.find(type));

            if(it == m_factories.end()) {
                throw registry_error_t("module '" + type + "' is not available");
            }

            if(it->second->category() != typeid(Category)) {
                throw registry_error_t("module '" + type + "' has an incompatible type");
            }

            return typename Category::policy::template pointer<Category>::type(
                dynamic_cast< factory_concept_t<Category>* >(
                    it->second
                )->create(m_context)
            );
        }

        template<class T, class Category>
        typename boost::enable_if<
            boost::is_base_of<Category, T>
        >::type 
        install(const std::string& type) {
            if(m_factories.find(type) != m_factories.end()) {
                throw registry_error_t("duplicate module '" + type + "' has been detected");
            }

            m_factories.insert(
                type,
                new factory_t<T, Category, typename Category::policy>()
            );
        }

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        // Used to unload all the modules on shutdown.
        std::vector<lt_dlhandle> m_modules;
    
#if BOOST_VERSION >= 104000
        typedef boost::ptr_unordered_map<
#else
        typedef boost::ptr_map<
#endif
            const std::string, 
            category_concept_t
        > factory_map_t;
        
        factory_map_t m_factories;
};

}}

#endif
