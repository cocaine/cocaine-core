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

#include <cocaine/repository.hpp>

#include <cocaine/forwards.hpp>

#include <cocaine/unicorn/writable.hpp>

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

    /**
     * Typedefs for used writable helpers
     * These provide future-like functionality.
     * Either value can be written inside via call to write,
     * or an error can be signaled via call to abort.
     * @see cocaine::unicorn::writable_adapter_base_t
     **/
    struct writable_ptr {
        typedef unicorn::writable_helper<response::put>::ptr put;
        typedef unicorn::writable_helper<response::create>::ptr create;
        typedef unicorn::writable_helper<response::del>::ptr del;
        typedef unicorn::writable_helper<response::increment>::ptr increment;
        typedef unicorn::writable_helper<response::get>::ptr get;
        typedef unicorn::writable_helper<response::subscribe>::ptr subscribe;
        typedef unicorn::writable_helper<response::children_subscribe>::ptr children_subscribe;
        typedef unicorn::writable_helper<response::lock>::ptr lock;
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
    put(writable_ptr::put result,
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
    get(writable_ptr::get result,
        const unicorn::path_t& path) = 0;

    /**
     * Create a node on specified path with specified value.
     */
    virtual
    unicorn_scope_ptr
    create(writable_ptr::create result,
           const unicorn::path_t& path,
           const unicorn::value_t& value,
           bool ephemeral,
           bool sequence) = 0;

    unicorn_scope_ptr
    create_default(writable_ptr::create result,
                   const unicorn::path_t& path,
                   const unicorn::value_t& value)
    {
        return create(std::move(result), path, value, false, false);
    }

    virtual
    unicorn_scope_ptr
    del(writable_ptr::del result,
        const unicorn::path_t& path,
        unicorn::version_t version) = 0;

    virtual
    unicorn_scope_ptr
    subscribe(writable_ptr::subscribe result,
              const unicorn::path_t& path) = 0;

    virtual
    unicorn_scope_ptr
    children_subscribe(writable_ptr::children_subscribe result,
                       const unicorn::path_t& path) = 0;

    virtual
    unicorn_scope_ptr
    increment(writable_ptr::increment result,
              const unicorn::path_t& path,
              const unicorn::value_t& value) = 0;

    virtual
    unicorn_scope_ptr
    lock(writable_ptr::lock result,
         const unicorn::path_t& path) = 0;

    virtual
    ~unicorn_t() {}
};

template<>
struct category_traits<unicorn_t> {
    typedef std::shared_ptr<unicorn_t> ptr_type;

    struct factory_type: public basic_factory<unicorn_t> {
        virtual
        ptr_type
        get(context_t& context, const std::string& name, const dynamic_t& args) = 0;
    };

    template<class T>
    struct default_factory: public factory_type {
        virtual
        ptr_type
        get(context_t& context, const std::string& name, const dynamic_t& args) {
            ptr_type instance;

            instances.apply([&](std::map<std::string, std::weak_ptr<unicorn_t>>& instances_) {
                if((instance = instances_[name].lock()) == nullptr) {
                    instance = std::make_shared<T>(context, name, args);
                    instances_[name] = instance;
                }
            });

            return instance;
        }

    private:
        synchronized<std::map<std::string, std::weak_ptr<unicorn_t>>> instances;
    };
};

category_traits<unicorn_t>::ptr_type
unicorn(context_t& context, const std::string& name);

}}

#endif // COCAINE_UNICORN_API_HPP
