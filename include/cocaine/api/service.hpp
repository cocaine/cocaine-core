/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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
#include "cocaine/json.hpp"
#include "cocaine/repository.hpp"

namespace cocaine { namespace api {

class service_t {
    public:
        typedef service_t category_type;

    public:
        virtual
        ~service_t() {
            // Empty.
        }

        virtual
        void
        run() = 0;

        virtual
        void
        terminate() = 0;

    protected:
        service_t(context_t&,
                  const std::string& /* name */,
                  const Json::Value& /* args */)
        { }
};

template<>
struct category_traits<service_t> {
    typedef std::unique_ptr<service_t> ptr_type;

    struct factory_type:
        public basic_factory<service_t>
    {
        virtual
        ptr_type
        get(context_t& context,
            const std::string& name,
            const Json::Value& args) = 0;
    };

    template<class T>
    struct default_factory:
        public factory_type
    {
        virtual
        ptr_type
        get(context_t& context,
            const std::string& name,
            const Json::Value& args)
        {
            return ptr_type(
                new T(context, name, args)
            );
        }
    };
};

category_traits<service_t>::ptr_type
service(context_t& context,
        const std::string& name);

}} // namespace cocaine::api

#endif
