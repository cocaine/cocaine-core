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

#ifndef COCAINE_DRIVER_API_HPP
#define COCAINE_DRIVER_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "json/json.h"

namespace cocaine { namespace api {

struct driver_t {
    typedef driver_t category_type;

    virtual
   ~driver_t() {
        // Empty.
    }

    virtual
    Json::Value
    info() const = 0;

protected:
    driver_t(context_t&, io::reactor_t&, app_t&, const std::string& /* name */, const Json::Value& /* args */) {
        // Empty.
    }
};

template<>
struct category_traits<driver_t> {
    typedef std::unique_ptr<driver_t> ptr_type;

    struct factory_type: public basic_factory<driver_t> {
        virtual
        ptr_type
        get(context_t& context, io::reactor_t& reactor, app_t& app, const std::string& name, const Json::Value& args) = 0;
    };

    template<class T>
    struct default_factory: public factory_type {
        virtual
        ptr_type
        get(context_t& context, io::reactor_t& reactor, app_t& app, const std::string& name, const Json::Value& args) {
            return ptr_type(new T(context, reactor, app, name, args));
        }
    };
};

}} // namespace cocaine::api

#endif
