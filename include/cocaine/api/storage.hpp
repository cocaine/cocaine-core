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

#ifndef COCAINE_STORAGE_API_HPP
#define COCAINE_STORAGE_API_HPP

#include "cocaine/common.hpp"

#include "cocaine/locked_ptr.hpp"
#include "cocaine/traits.hpp"
#include "cocaine/utility/future.hpp"

#include <sstream>

namespace cocaine { namespace api {

struct storage_t {
    typedef storage_t category_type;

    template<class T>
    using callback = std::function<void(std::future<T>)>;

    virtual
   ~storage_t() {
        // Empty.
    }

    virtual
    void
    read(const std::string& collection, const std::string& key, callback<std::string> cb) = 0;

    virtual
    std::future<std::string>
    read(const std::string& collection, const std::string& key);

    virtual
    void
    write(const std::string& collection,
          const std::string& key,
          const std::string& blob,
          const std::vector<std::string>& tags,
          callback<void> cb) = 0;

    virtual
    std::future<void>
    write(const std::string& collection,
          const std::string& key,
          const std::string& blob,
          const std::vector<std::string>& tags);

    virtual
    void
    remove(const std::string& collection, const std::string& key, callback<void> cb) = 0;

    virtual
    std::future<void>
    remove(const std::string& collection, const std::string& key);

    virtual
    void
    find(const std::string& collection, const std::vector<std::string>& tags, callback<std::vector<std::string>> cb) = 0;

    virtual
    std::future<std::vector<std::string>>
    find(const std::string& collection, const std::vector<std::string>& tags);

    // Helper methods

    template<class T>
    void
    get(const std::string& collection, const std::string& key, callback<T> cb);

    template<class T>
    std::future<T>
    get(const std::string& collection, const std::string& key);

    template<class T>
    void
    put(const std::string& collection,
        const std::string& key,
        const T& object,
        const std::vector<std::string>& tags,
        callback<void> cb);

    template<class T>
    std::future<void>
    put(const std::string& collection,
        const std::string& key,
        const T& object,
        const std::vector<std::string>& tags);


protected:
    storage_t(context_t&, const std::string& /* name */, const dynamic_t& /* args */) {
        // Empty.
    }
private:
    template <class T>
    static
    void assign_future_result(std::promise<T>& promise, std::future<T> future) {
        try {
            promise.set_value(future.get());
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }

    static
    void assign_future_result(std::promise<void>& promise, std::future<void> future) {
        try {
            future.get();
            promise.set_value();
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    }
};

template<class T>
void
storage_t::get(const std::string& collection, const std::string& key, callback<T> cb) {

    // TODO: move inside lambda as we move on c++14
    auto inner_cb = [=](std::future<std::string> f) {
        T result;
        msgpack::unpacked unpacked;
        std::string blob;

        try {
            blob = f.get();
            msgpack::unpack(&unpacked, blob.data(), blob.size());
            io::type_traits<T>::unpack(unpacked.get(), result);
        } catch(...) {
            return cb(make_exceptional_future<T>());
        }

        return cb(make_ready_future(result));
    };
    read(collection, key, std::move(inner_cb));
}

template<class T>
std::future<T>
storage_t::get(const std::string& collection, const std::string& key) {
    auto promise = std::make_shared<std::promise<T>>();
    get<T>(collection, key, [=](std::future<T> future){
        assign_future_result<T>(*promise, std::move(future));
    });
    return promise->get_future();
}

template<class T>
void
storage_t::put(const std::string& collection,
               const std::string& key,
               const T& object,
               const std::vector<std::string>& tags,
               callback<void> cb)
{
    std::ostringstream buffer;
    msgpack::packer<std::ostringstream> packer(buffer);

    io::type_traits<T>::pack(packer, object);

    write(collection, key, buffer.str(), tags, std::move(cb));
}

template<class T>
std::future<void>
storage_t::put(const std::string& collection, const std::string& key, const T& object, const std::vector<std::string>& tags) {
    auto promise = std::make_shared<std::promise<void>>();
    put(collection, key, object, tags, [=](std::future<void> future) mutable {
        assign_future_result(*promise, std::move(future));
    });
    return promise->get_future();
}


typedef std::shared_ptr<storage_t> storage_ptr;

storage_ptr
storage(context_t& context, const std::string& name);

}} // namespace cocaine::api

#endif
