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
        struct ptr {
            typedef std::auto_ptr<T> type;
        };
    };

    struct share {
        template<class T>
        struct ptr {
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
    public:
        typedef typename Category::policy::
            template ptr<Category>::type 
        ptr_type;
    
    public:
        virtual const std::type_info& category() const {
            return typeid(Category);
        }

        virtual ptr_type get(context_t&) = 0;
};

template<class T, class Category, class RetentionPolicy>
class factory_t;

template<class T, class Category>
class factory_t<T, Category, policies::none>:
    public factory_concept_t<Category>
{
    public:
        virtual
        typename factory_concept_t<Category>::ptr_type
        get(context_t& context) {
            return typename factory_concept_t<Category>::ptr_type(
                new T(context)
            );
        }
};

template<class T, class Category>
class factory_t<T, Category, policies::share>:
    public factory_concept_t<Category>
{
    public:
        virtual
        typename factory_concept_t<Category>::ptr_type
        get(context_t& context) {
            boost::lock_guard<boost::mutex> lock(m_mutex);

            if(!m_instance) {
                m_instance.reset(new T(context));
            }

            return m_instance;
        }

    private:
        boost::mutex m_mutex;
        typename factory_concept_t<Category>::ptr_type m_instance;
};

// Component registry
// ------------------

class registry_t:
    public boost::noncopyable
{
    public:
        registry_t(context_t& context);
        ~registry_t();

        template<class Category>
        typename factory_concept_t<Category>::ptr_type
        get(const std::string& type) {
            factory_map_t::iterator it(m_factories.find(type));

            if(it == m_factories.end()) {
                throw registry_error_t("module '" + type + "' is not available");
            }

            if(it->second->category() != typeid(Category)) {
                throw registry_error_t("module '" + type + "' has an incompatible type");
            }

            return typename factory_concept_t<Category>::ptr_type(
                dynamic_cast< factory_concept_t<Category>* >(
                    it->second
                )->get(m_context)
            );
        }

        template<class T, class Category>
        typename boost::enable_if<
            boost::is_base_of<Category, T>
        >::type 
        insert(const std::string& type) {
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
