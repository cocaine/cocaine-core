/*
* 2015+ Copyright (c) Anton Matveenko <antmat@me.com>
* All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#ifndef COCAINE_UNICORN_API_HPP
#define COCAINE_UNICORN_API_HPP

#include "cocaine/common.hpp"

#include <future>
#include <vector>

namespace cocaine { namespace unicorn {

typedef long long version_t;
typedef std::string path_t;
typedef cocaine::dynamic_t value_t;
class versioned_value_t;

}} //namespace cocaine::unicorn

namespace cocaine { namespace api {

/**
 * Unicorn request scope.
 *
 * As far as unicorn provides subscription/locking functionality there is a need to control it.
 * This class is dedicated to manage lifetime of such subscriptions, locks or any other action which can be cancelled.
 **/
class unicorn_scope_t {
public:
    virtual void
    close() = 0;

    virtual
    ~unicorn_scope_t(){}
};

typedef std::shared_ptr<unicorn_scope_t> unicorn_scope_ptr;

//TODO: in v12.15 change
class auto_scope_t {
    unicorn_scope_ptr wrapped;
public:
    auto_scope_t() = default;
    auto_scope_t(unicorn_scope_ptr wrapped);
    auto_scope_t(const auto_scope_t&) = delete;
    auto_scope_t& operator=(const auto_scope_t&) = delete;
    auto_scope_t(auto_scope_t&&);
    auto_scope_t& operator=(auto_scope_t&&);
    ~auto_scope_t();
private:
    auto close() -> void;
};

/**
 * Unicorn API
 */
class unicorn_t {
public:
    typedef unicorn_t category_type;

    /**
     * Typedefs for result type.
     **/
    struct response {
        typedef std::tuple<bool, unicorn::versioned_value_t> put;
        typedef bool create;
        typedef bool del;
        typedef unicorn::versioned_value_t increment;
        typedef unicorn::versioned_value_t get;
        typedef unicorn::versioned_value_t subscribe;
        typedef std::tuple<unicorn::version_t, std::vector<std::string>> children_subscribe;
        typedef bool lock;
    };

    struct callback {
        typedef std::function<void(std::future<response::put>)> put;
        typedef std::function<void(std::future<response::create>)> create;
        typedef std::function<void(std::future<response::del>)> del;
        typedef std::function<void(std::future<response::increment>)> increment;
        typedef std::function<void(std::future<response::get>)> get;
        typedef std::function<void(std::future<response::subscribe>)> subscribe;
        typedef std::function<void(std::future<response::children_subscribe>)> children_subscribe;
        typedef std::function<void(std::future<response::lock>)> lock;
    };

    unicorn_t(context_t& context, const std::string& name, const dynamic_t& args);

    /**
     * Put value in unicorn.
     * @param result writable to which will be result of operation written.
     * @param path path to put value in with starting '/'.
     * @param value value to put in path.
     * @param version current version in unicorn. If it do not match - unicorn will return an error.
     */
    virtual
    unicorn_scope_ptr
    put(callback::put callback,
        const unicorn::path_t& path,
        const unicorn::value_t& value,
        unicorn::version_t version) = 0;

    /**
     * Get value from unicorn.
     * @param result writable to which will be result of operation written.
     * @param path path to read value from.
     */
    virtual
    unicorn_scope_ptr
    get(callback::get callback,
        const unicorn::path_t& path) = 0;

    /**
     * Create a node on specified path with specified value.
     */
    virtual
    unicorn_scope_ptr
    create(callback::create callback,
           const unicorn::path_t& path,
           const unicorn::value_t& value,
           bool ephemeral,
           bool sequence) = 0;

    unicorn_scope_ptr
    create_default(callback::create callback,
                   const unicorn::path_t& path,
                   const unicorn::value_t& value)
    {
        return create(std::move(callback), path, value, false, false);
    }

    virtual
    unicorn_scope_ptr
    del(callback::del callback,
        const unicorn::path_t& path,
        unicorn::version_t version) = 0;

    virtual
    unicorn_scope_ptr
    subscribe(callback::subscribe callback,
              const unicorn::path_t& path) = 0;

    virtual
    unicorn_scope_ptr
    children_subscribe(callback::children_subscribe callback,
                       const unicorn::path_t& path) = 0;

    virtual
    unicorn_scope_ptr
    increment(callback::increment callback,
              const unicorn::path_t& path,
              const unicorn::value_t& value) = 0;

    // TODO: deprecate or remove in 12.15 in favour of named_lock
    // See cocaine-plugins/unicorn/include/api/unicorn.hpp
    virtual
    unicorn_scope_ptr
    lock(callback::lock callback,
         const unicorn::path_t& path) = 0;

    virtual
    ~unicorn_t() {}
};

typedef std::shared_ptr<unicorn_t> unicorn_ptr;

unicorn_ptr
unicorn(context_t& context, const std::string& name);

}}

#endif // COCAINE_UNICORN_API_HPP
