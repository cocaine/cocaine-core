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

#include "cocaine/api/storage.hpp"

#include "cocaine/context.hpp"

namespace cocaine {

struct source {
    enum value { cache, storage };
};

template<class T>
struct cached:
    protected T
{
    cached(context_t& context, const std::string& collection, const std::string& name);

    source::value
    source() const {
        return m_source;
    }

private:
    void
    download(context_t& context, const std::string& collection, const std::string& name);

private:
    source::value m_source;
};

template<class T>
cached<T>::cached(context_t& context, const std::string& collection, const std::string& name) {
    api::category_traits<api::storage_t>::ptr_type cache;

    try {
        cache = api::storage(context, "cache");
    } catch(const api::repository_error_t& e) {
        download(context, collection, name);
        return;
    }

    T& object = static_cast<T&>(*this);

    try {
        object = cache->get<T>(collection, name);
    } catch(const storage_error_t& e) {
        download(context, collection, name);

        try {
            cache->put(collection, name, object, std::vector<std::string>());
        } catch(const storage_error_t& e) {
            throw storage_error_t("unable to cache object '%s' in '%s' - %s", name, collection, e.what());
        }

        return;
    }

    m_source = source::cache;
}

template<class T>
void
cached<T>::download(context_t& context, const std::string& collection, const std::string& name) {
    T& object = static_cast<T&>(*this);

    // Intentionally propagate storage exceptions from this call.
    object = api::storage(context, "core")->get<T>(collection, name);

    m_source = source::storage;
}

} // namespace cocaine

#endif
