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

namespace cocaine { namespace api {

template<class Category>
struct category_traits;

struct factory_concept_t {
    virtual
    ~factory_concept_t() {
        // Empty.
    }
    
    virtual
    const std::type_info&
    id() const = 0;
};

template<class Category>
struct factory_base:
    public factory_concept_t
{
    virtual
    const std::type_info&
    id() const {
        return typeid(Category);
    }
};

// Customized plugin instantiation

template<class T>
struct plugin_traits {
    typedef typename category_traits<
        typename T::category_type
    >::template default_factory<T> factory_type;
};

// Component repository

struct repository_error_t:
    public error_t
{
    template<typename... Args>
    repository_error_t(const std::string& format,
                       const Args&... args):
        error_t(format, args...)
    { }
};

class repository_t:
    public boost::noncopyable
{
    public:
        repository_t(context_t& context);
        ~repository_t();

        void
        load(const std::string& path);

        template<class Category, typename... Args>
        typename category_traits<Category>::ptr_type
        get(const std::string& type,
            Args&&... args);

        template<class T, class Category = typename T::category_type>
        void
        insert(const std::string& type);

    private:
        void
        open(const std::string& target);

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        // NOTE: Used to unload all the plugins on shutdown.
        // Cannot use a forward declaration here due to the implementation
        // details.
        std::vector<lt_dlhandle> m_plugins;

#if BOOST_VERSION >= 104000
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            std::string,
            std::unique_ptr<factory_concept_t>
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

template<class Category, typename... Args>
typename category_traits<Category>::ptr_type
repository_t::get(const std::string& type,
                  Args&&... args)
{
    std::string id = typeid(Category).name();

    factory_map_t& factories = m_categories[id];
    factory_map_t::iterator it(factories.find(type));
    
    if(it == factories.end()) {
        throw repository_error_t("the '%s' component is not available", type);
    }
    
    // TEST: Ensure that the plugin is of the actually specified category.
    BOOST_ASSERT(it->second->id() == typeid(Category));
    
    typedef category_traits<Category> traits;

    return typename traits::ptr_type(
        dynamic_cast< typename traits::factory_type& >(
            *it->second
        ).get(m_context, std::forward<Args>(args)...)
    );
}

template<class T, class Category>
void
repository_t::insert(const std::string& type) {
    static_assert(
        boost::is_base_of<
            Category,
            T
        >::value,
        "component is not derived from its category"
    );

    static_assert(
        boost::is_base_of<
            typename category_traits<Category>::factory_type,
            typename plugin_traits<T>::factory_type
        >::value,
        "component factory is not derived from its category"
    );

    factory_map_t& factories = m_categories[typeid(Category).name()];

    if(factories.find(type) != factories.end()) {
        throw repository_error_t("the '%s' component is a duplicate", type);
    }

    factories.emplace(
        type,
        new typename plugin_traits<T>::factory_type()
    );
}

typedef void (*initialize_fn_t)(repository_t&);

}} // namespace cocaine::api

#endif
