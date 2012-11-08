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
#include <boost/utility/enable_if.hpp>
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
    category() const = 0;
};

// Factory interface

template<
    class Category,
    class Traits = category_traits<Category>
>
struct factory:
    public factory_concept_t
{
    virtual
    const std::type_info&
    category() const {
        return typeid(Category);
    }

    virtual
    typename Traits::ptr_type
    get(context_t& context,
        const typename Traits::args_type& args) = 0;
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

        template<class Category>
        typename category_traits<Category>::ptr_type
        get(const std::string& type,
            const typename category_traits<Category>::args_type& args);

        template<class T>
        typename boost::enable_if<
            boost::is_base_of<typename T::category_type, T>
        >::type 
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

        factory_map_t m_factories;
};

template<class Category>
typename category_traits<Category>::ptr_type
repository_t::get(const std::string& type,
                  const typename category_traits<Category>::args_type& args)
{
    factory_map_t::iterator it(m_factories.find(type));
    
    if(it == m_factories.end()) {
        throw repository_error_t("the '%s' component is not available", type);
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
repository_t::insert(const std::string& type) {
    if(m_factories.find(type) != m_factories.end()) {
        throw repository_error_t("the '%s' component is a duplicate", type);
    }

    m_factories.emplace(
        type,
        new typename plugin_traits<T>::factory_type()
    );
}

typedef void (*initialize_fn_t)(repository_t&);

}} // namespace cocaine::api

#endif
