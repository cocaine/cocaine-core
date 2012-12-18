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

#ifndef COCAINE_SANDBOX_API_HPP
#define COCAINE_SANDBOX_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace api {

class sandbox_t:
    public boost::noncopyable
{
    public:
        virtual
        ~sandbox_t() {
            // Empty.
        }

        virtual
        boost::shared_ptr<stream_t>
        invoke(const std::string& event,
               const boost::shared_ptr<stream_t>& upstream) = 0;

    protected:
        sandbox_t(context_t&,
                  const std::string& /* name */,
                  const Json::Value& /* args */,
                  const std::string& /* spool */)
        { }
};

template<>
struct category_traits<sandbox_t> {
    typedef std::unique_ptr<sandbox_t> ptr_type;

    struct factory_type:
        public factory_base<sandbox_t>
    {
        virtual
        ptr_type
        get(context_t& context,
            const std::string& name,
            const Json::Value& args,
            const std::string& spool) = 0;
    };

    template<class T>
    struct default_factory:
        public factory_type
    {
        virtual
        ptr_type 
        get(context_t& context,
            const std::string& name,
            const Json::Value& args,
            const std::string& spool)
        {
            return ptr_type(
                new T(context, name, args, spool)
            );
        }
    };
};

}} // namespace cocaine::api

#endif
