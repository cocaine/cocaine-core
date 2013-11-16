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

#ifndef COCAINE_SERVICE_API_HPP
#define COCAINE_SERVICE_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/dynamic.hpp"
#include "cocaine/repository.hpp"

namespace cocaine { namespace api {

struct service_t {
    typedef service_t category_type;

    virtual
   ~service_t() {
        // Empty.
    }

    virtual
    auto
    prototype() -> io::dispatch_t& = 0;

protected:
    service_t(context_t&, io::reactor_t&, const std::string& /* name */, const dynamic_t& /* args */) {
        // Empty.
    }
};

template<>
struct category_traits<service_t> {
    typedef std::unique_ptr<service_t> ptr_type;

    struct factory_type: public basic_factory<service_t> {
        virtual
        ptr_type
        get(context_t& context, io::reactor_t& reactor, const std::string& name, const dynamic_t& args) = 0;
    };

    template<class T>
    struct default_factory: public factory_type {
        virtual
        ptr_type
        get(context_t& context, io::reactor_t& reactor, const std::string& name, const dynamic_t& args) {
            return ptr_type(new T(context, reactor, name, args));
        }
    };
};

}} // namespace cocaine::api

#endif
