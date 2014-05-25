/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/common.hpp"

#include <typeinfo>
#include <type_traits>

#include <ltdl.h>

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
struct basic_factory:
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
    typedef typename category_traits<typename T::category_type>::template default_factory<T>
            factory_type;
};

// Component repository

struct repository_error_t:
    public cocaine::error_t
{
    template<typename... Args>
    repository_error_t(const std::string& format, const Args&... args):
        cocaine::error_t(format, args...)
    { }
};

class repository_t {
    COCAINE_DECLARE_NONCOPYABLE(repository_t)

    // NOTE: Used to unload all the plugins on shutdown.
    // Cannot use a forward declaration here due to the implementation details.
    std::vector<lt_dlhandle> m_plugins;

    typedef std::map<std::string, std::shared_ptr<factory_concept_t>> factory_map_t;
    typedef std::map<std::string, factory_map_t> category_map_t;

    category_map_t m_categories;

public:
    repository_t();
   ~repository_t();

    void
    load(const std::string& path);

    template<class Category, typename... Args>
    typename category_traits<Category>::ptr_type
    get(const std::string& type, Args&&... args);

    template<class T>
    void
    insert(const std::string& type);

private:
    void
    open(const std::string& target);
};

template<class Category, typename... Args>
typename category_traits<Category>::ptr_type
repository_t::get(const std::string& type, Args&&... args) {
    const std::string id = typeid(Category).name();
    const factory_map_t& factories = m_categories[id];

    factory_map_t::const_iterator it = factories.find(type);

    if(it == factories.end()) {
        throw repository_error_t("the '%s' component is not available", type);
    }

    // TEST: Ensure that the plugin is of the actually specified category.
    BOOST_ASSERT(it->second->id() == typeid(Category));

    return dynamic_cast<typename category_traits<Category>::factory_type&>(
        *it->second
    ).get(std::forward<Args>(args)...);
}

template<class T>
void
repository_t::insert(const std::string& type) {
    typedef typename T::category_type category_type;
    typedef typename plugin_traits<T>::factory_type factory_type;

    static_assert(
        std::is_base_of<category_type, T>::value,
        "component is not derived from its category"
    );

    static_assert(
        std::is_base_of<typename category_traits<category_type>::factory_type, factory_type>::value,
        "component factory is not derived from its category"
    );

    const std::string id = typeid(category_type).name();
    factory_map_t& factories = m_categories[id];

    if(factories.find(type) != factories.end()) {
        throw repository_error_t("the '%s' component is a duplicate", type);
    }

    factories[type] = std::make_shared<factory_type>();
}

struct preconditions_t {
    unsigned version;
};

}} // namespace cocaine::api

#endif
