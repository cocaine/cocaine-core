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

#ifndef COCAINE_REPOSITORY_UNICORN_HPP
#define COCAINE_REPOSITORY_UNICORN_HPP

#include "cocaine/api/unicorn.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/repository.hpp"

#include <future>

namespace cocaine { namespace api {

template<>
struct category_traits<unicorn_t> {
    typedef unicorn_ptr ptr_type;

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

}}

#endif // COCAINE_UNICORN_API_HPP
