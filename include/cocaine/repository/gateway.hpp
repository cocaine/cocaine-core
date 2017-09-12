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

#ifndef COCAINE_REPOSITORY_GATEWAY_HPP
#define COCAINE_REPOSITORY_GATEWAY_HPP

#include "cocaine/api/gateway.hpp"
#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

namespace cocaine { namespace api {

template<>
struct category_traits<gateway_t> {
    typedef gateway_ptr ptr_type;

    struct factory_type: public basic_factory<gateway_t> {
        virtual
        ptr_type
        get(context_t& context, const std::string& uuid, const std::string& name, const dynamic_t& args,
            const dynamic_t::object_t& locator_extra) = 0;
    };

    template<class T>
    struct default_factory: public factory_type {
        virtual
        ptr_type
        get(context_t& context, const std::string& uuid, const std::string& name, const dynamic_t& args,
            const dynamic_t::object_t& locator_extra)
        {
            return ptr_type(new T(context, uuid, name, args, locator_extra));
        }
    };
};

}} // namespace cocaine::api

#endif
