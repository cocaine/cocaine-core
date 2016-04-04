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
#include "cocaine/errors.hpp"
#include "cocaine/memory.hpp"

#include <boost/assert.hpp>

#include <map>
#include <typeinfo>
#include <type_traits>
#include <vector>

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
    auto
    type_id() const -> const std::type_info& = 0;
};

template<class Category>
struct basic_factory:
    public factory_concept_t
{
    virtual
    auto
    type_id() const -> const std::type_info& {
        return typeid(Category);
    }
};

// Customized plugin instantiation

template<class T>
struct plugin_traits {
    typedef category_traits<typename T::category_type> traits_type;

    // By default, all plugins are instantiated by the default factory for their category. This can
    // be customized by specializing this template.
    typedef typename traits_type::template default_factory<T> factory_type;
};

// Component repository

class repository_t {
    COCAINE_DECLARE_NONCOPYABLE(repository_t)

    const std::unique_ptr<logging::logger_t> m_log;

    // NOTE: Used to unload all the plugins on shutdown. Cannot use a forward declaration here due
    // to the implementation details.
    std::vector<lt_dlhandle> m_plugins;

    typedef std::map<std::string, std::unique_ptr<factory_concept_t>> factory_map_t;
    typedef std::map<std::string, factory_map_t> category_map_t;

    category_map_t m_categories;

public:
    explicit
    repository_t(std::unique_ptr<logging::logger_t> log);

   ~repository_t();

    void
    load(const std::vector<std::string>& plugin_dirs);

    template<class Category, class... Args>
    typename category_traits<Category>::ptr_type
    get(const std::string& name, Args&&... args) const;

    template<class T>
    void
    insert(const std::string& name);

private:
    void
    open(const std::string& target);

    void
    insert(const std::string& id, const std::string& name, std::unique_ptr<factory_concept_t> factory);
};

template<class Category, class... Args>
typename category_traits<Category>::ptr_type
repository_t::get(const std::string& name, Args&&... args) const {
    const auto id = typeid(Category).name();

    if(!m_categories.count(id) || !m_categories.at(id).count(name)) {
        throw std::system_error(error::component_not_found, format("{}[{}]", name, typeid(Category).name()));
    }

    auto it = m_categories.at(id).find(name);

    // TEST: Ensure that the plugin is of the actually specified category.
    BOOST_ASSERT(it->second->type_id() == typeid(Category));

    return dynamic_cast<typename category_traits<Category>::factory_type&>(
        *it->second
    ).get(std::forward<Args>(args)...);
}

template<class T>
void
repository_t::insert(const std::string& name) {
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

    insert(typeid(category_type).name(), name, std::make_unique<factory_type>());
}

struct preconditions_t {
    unsigned version;
};

}} // namespace cocaine::api

#endif
