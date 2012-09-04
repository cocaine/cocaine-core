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

#include <boost/tuple/tuple.hpp>

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace api {

class service_t:
    public boost::noncopyable
{
    public:
        virtual
        ~service_t() {
            // Empty.
        }

        virtual
        void
        run() = 0;
    
    protected:
        service_t(context_t& context, const std::string&, const Json::Value&):
            m_context(context)
        { }
        
    private:
        context_t& m_context;
};

template<>
struct category_traits<api::service_t> {
    typedef std::auto_ptr<api::service_t> ptr_type;

    typedef boost::tuple<
        const std::string&,
        const Json::Value&
    > args_type;

    template<class T>
    struct default_factory:
        public factory<api::service_t>
    {
        virtual
        ptr_type
        get(context_t& context,
            const args_type& args)
        {
            return ptr_type(
                new T(
                    context,
                    boost::get<0>(args),
                    boost::get<1>(args)
                )
            );
        }
    };
};

}}

#endif
