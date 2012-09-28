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

#ifndef COCAINE_CACHED_HPP
#define COCAINE_CACHED_HPP

#include "cocaine/common.hpp"
#include "cocaine/context.hpp"

#include "cocaine/api/storage.hpp"

namespace cocaine {

template<class T>
struct cached {
    cached(context_t& context, const std::string& collection, const std::string& name):
        m_context(context),
        m_collection(collection),
        m_name(name)
    {
        api::category_traits<api::storage_t>::ptr_type cache(
            m_context.get<api::storage_t>("storage/cache")
        );

        try {
            // Try to load the object from the cache.
            m_object = cache->get<T>(collection, name);
        } catch(const storage_error_t& e) {
            update();

            try {
                // Put the application object into the cache for future reference.
                cache->put(collection, name, m_object);
            } catch(const storage_error_t& e) {
                throw storage_error_t("unable to cache the '" + name + "' object");
            }    
        }
    }

    const T& object() const {
        return m_object;
    }

private:
    void
    update() {
        api::category_traits<api::storage_t>::ptr_type storage(
            m_context.get<api::storage_t>("storage/core")
        );
        
        try {
            // Fetch the application manifest and archive from the core storage.
            m_object = storage->get<T>(m_collection, m_name);
        } catch(const storage_error_t& e) {
            throw configuration_error_t("unable to fetch the '" + m_name + "' object from the storage");
        }
    }

private:
    context_t& m_context;
    boost::shared_ptr<logging::logger_t> m_log;

    const std::string m_collection,
                      m_name;

    // The actual cached object.
    T m_object;
};

} // namespace cocaine

#endif
