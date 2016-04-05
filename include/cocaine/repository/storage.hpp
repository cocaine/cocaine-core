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

#ifndef COCAINE_REPOSITORY_STORAGE_HPP
#define COCAINE_REPOSITORY_STORAGE_HPP

#include "cocaine/api/storage.hpp"
#include "cocaine/common.hpp"
#include "cocaine/dynamic/dynamic.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/repository.hpp"

#include <map>
#include <memory>
#include <string>

namespace cocaine { namespace api {

template<>
struct category_traits<storage_t> {
    typedef storage_ptr ptr_type;

    struct factory_type: public basic_factory<storage_t> {
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

            instances.apply([&](std::map<std::string, std::weak_ptr<storage_t>>& instances) {
                if((instance = instances[name].lock()) == nullptr) {
                    instance = std::make_shared<T>(context, name, args);
                    instances[name] = instance;
                }
            });

            return instance;
        }

    private:
        synchronized<std::map<std::string, std::weak_ptr<storage_t>>> instances;
    };
};


}} // namespace cocaine::api

#endif
