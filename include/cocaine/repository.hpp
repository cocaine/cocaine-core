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

#ifndef COCAINE_REPOSITORY_HPP
#define COCAINE_REPOSITORY_HPP

#include <boost/type_traits/is_base_of.hpp>
#include <ltdl.h>
#include <typeinfo>

#include "cocaine/common.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine {

template<class Category>
struct category_traits;

struct factory_concept_t {
    virtual ~factory_concept_t() = 0;
    virtual const std::type_info& category() const = 0;
};

// Custom factories should inherit from this interface.
template<
    class Category,
    class Traits = category_traits<Category>
>
struct factory:
    public factory_concept_t
{
    typedef typename Traits::ptr_type ptr_type;
    typedef typename Traits::args_type args_type;

    virtual const std::type_info& category() const {
        return typeid(Category);
    }

    virtual ptr_type get(context_t& context,
                         const args_type& args) = 0;
};

// Specialize this in your plugin to use
// a custom factory for your object instantiations.
template<class T>
struct plugin_traits {
    typedef typename category_traits<
        typename T::category_type
    >::template default_factory<T> factory_type;
};

// Plugin configuration
// -------------------- 

struct plugin_config_t {
    const std::string name;
    const Json::Value args;
};

// Component repository
// --------------------

class repository_t:
    public boost::noncopyable
{
    public:
        repository_t(context_t& context);
        ~repository_t();

        template<class Category>
        typename category_traits<Category>::ptr_type
        get(const std::string& type,
            const typename category_traits<Category>::args_type& args)
        {
            std::string category(
                typeid(Category).name()
            );
            
            factory_map_t::iterator it(m_categories[category].find(type));
            
            if(it == m_categories[category].end()) {
                throw repository_error_t("the '" + type + "' plugin is not available");
            }
            
            // TEST: Ensure that the plugin is of the actually specified category.
            BOOST_ASSERT(it->second->category() == typeid(Category));

            return typename category_traits<Category>::ptr_type(
                dynamic_cast< factory<Category>& >(
                    *it->second
                ).get(m_context, args)
            );
        }

        template<class T>
        typename boost::enable_if<
            boost::is_base_of<typename T::category_type, T>
        >::type 
        insert(const std::string& type) {
            std::string category(
                typeid(typename T::category_type).name()
            );

            if(m_categories[category].find(type) != m_categories[category].end()) {
                throw repository_error_t("the '" + type + "' plugin is a duplicate");
            }

            m_categories[category].insert(
                std::make_pair(
                    type,
                    boost::make_shared<typename plugin_traits<T>::factory_type>()
                )
            );
        }

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        // Used to unload all the plugins on shutdown.
        std::vector<lt_dlhandle> m_plugins;
    
#if BOOST_VERSION >= 104000
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            std::string, 
            boost::shared_ptr<factory_concept_t>
        > factory_map_t;

#if BOOST_VERSION >= 104000
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            std::string,
            factory_map_t
        > category_map_t;
        
        category_map_t m_categories;
};

}

#endif
