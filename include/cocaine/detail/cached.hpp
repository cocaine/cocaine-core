/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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
struct cached:
    protected T
{
    cached(context_t& context, const std::string& collection, const std::string& name);
};

template<class T>
cached<T>::cached(context_t& context, const std::string& collection, const std::string& name) {
    T& object = static_cast<T&>(*this);

    auto cache = api::storage(context, "cache");

    try {
        // Try to load the object from the cache.
        object = cache->get<T>(collection, name);
    } catch(const storage_error_t& e) {
        auto storage = api::storage(context, "core");

        try {
            // Fetch the application manifest and archive from the core storage.
            object = storage->get<T>(collection, name);
        } catch(const storage_error_t& e) {
            throw storage_error_t(
                "unable to fetch the '%s/%s' object from the storage - %s",
                collection,
                name,
                e.what()
            );
        }

        try {
            // Put the application object into the cache for future reference.
            cache->put(collection, name, object);
        } catch(const storage_error_t& e) {
            throw storage_error_t(
                "unable to cache the '%s/%s' object - %s",
                collection,
                name,
                e.what()
            );
        }
    }
}

} // namespace cocaine

#endif
