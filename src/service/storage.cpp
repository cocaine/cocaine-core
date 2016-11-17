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

#include "cocaine/detail/service/storage.hpp"

#include "cocaine/api/storage.hpp"
#include "cocaine/dynamic/dynamic.hpp"

using namespace cocaine;
using namespace cocaine::io;
using namespace cocaine::service;

namespace ph = std::placeholders;

namespace {

template<class T>
void
abort_deferred(cocaine::deferred<T>& def, const std::error_code& ec, const std::string& reason) {
    try {
        def.abort(ec, reason);
    } catch (const std::system_error&) {
        // pass, the only reason of exception is detached session
    }
}

}  // namespace

storage_t::storage_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args):
    category_type(context, asio, name, args),
    dispatch<storage_tag>(name)
{
    const auto storage = api::storage(context, args.as_object().at("backend", "core").as_string());

    on<storage::read>().execute([=](const std::string& collection, const std::string& key) {
        deferred<std::string> result;
        storage->read(collection, key, [=](std::future<std::string> future) mutable {
            try {
                result.write(future.get());
            } catch (const std::system_error& e) {
                abort_deferred(result, e.code(), e.what());
            }
        });

        return result;
    });

    on<storage::write>().execute([=](const std::string& collection,
                                     const std::string& key,
                                     const std::string& blob,
                                     const std::vector<std::string>& tags)
    {
        deferred<void> result;
        storage->write(collection, key, blob, tags, [=](std::future<void> future) mutable {
            try {
                future.get();
                result.close();
            } catch (const std::system_error& e) {
                abort_deferred(result, e.code(), e.what());
            }
        });

        return result;
    });

    on<storage::remove>().execute([=](const std::string& collection, const std::string& key) {
        deferred<void> result;
        storage->remove(collection, key, [=](std::future<void> future) mutable {
            try {
                future.get();
                result.close();
            } catch (const std::system_error& e) {
                abort_deferred(result, e.code(), e.what());
            }
        });

        return result;
    });

    on<storage::find>().execute([=](const std::string& collection, const std::vector<std::string>& tags) {
        deferred<std::vector<std::string>> result;
        storage->find(collection, tags, [=](std::future<std::vector<std::string>> future) mutable {
            try {
                result.write(future.get());
            } catch (const std::system_error& e) {
                abort_deferred(result, e.code(), e.what());
            }
        });

        return result;
    });
}

const basic_dispatch_t&
storage_t::prototype() const {
    return *this;
}
