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

#ifndef COCAINE_GATEWAY_API_HPP
#define COCAINE_GATEWAY_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

// TODO: Drop this.
#include "cocaine/idl/locator.hpp"

#include "json/json.h"

namespace cocaine { namespace api {

struct gateway_t {
    typedef gateway_t category_type;

    virtual
   ~gateway_t() {
        // Empty.
    }

    typedef io::event_traits<io::locator::resolve>::result_type metadata_t;

    virtual
    metadata_t
    resolve(const std::string& name) const = 0;

    virtual
    void
    consume(const std::string& uuid, const std::string& name, const metadata_t& meta) = 0;

    virtual
    void
    cleanup(const std::string& uuid, const std::string& name) = 0;

protected:
    gateway_t(context_t&, const std::string& /* name */, const Json::Value& /* args */) {
        // Empty.
    }
};

template<>
struct category_traits<gateway_t> {
    typedef std::unique_ptr<gateway_t> ptr_type;

    struct factory_type: public basic_factory<gateway_t> {
        virtual
        ptr_type
        get(context_t& context, const std::string& name, const Json::Value& args) = 0;
    };

    template<class T>
    struct default_factory: public factory_type {
        virtual
        ptr_type
        get(context_t& context, const std::string& name, const Json::Value& args) {
            return ptr_type(new T(context, name, args));
        }
    };
};

}} // namespace cocaine::api

#endif
