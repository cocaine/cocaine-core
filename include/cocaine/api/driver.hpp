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

#ifndef COCAINE_DRIVER_API_HPP
#define COCAINE_DRIVER_API_HPP

#include <boost/tuple/tuple.hpp>

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace api {

class driver_t:
    public boost::noncopyable
{
    public:
        virtual
        ~driver_t() {
            // Empty.
        }

        virtual
        Json::Value
        info() const = 0;

    public:
        engine::engine_t&
        engine() {
            return m_engine;
        }

    protected:
        driver_t(context_t& context,
                 engine::engine_t& engine,
                 const std::string& /* name */,
                 const Json::Value& /* args */):
            m_engine(engine)
        {
            // Empty.
        }
        
    private:
        engine::engine_t& m_engine;
};

template<>
struct category_traits<api::driver_t> {
    typedef std::unique_ptr<api::driver_t> ptr_type;

    typedef boost::tuple<
        engine::engine_t&,
        const std::string&,
        const Json::Value&
    > args_type;

    template<class T>
    struct default_factory:
        public factory<api::driver_t>
    {
        virtual
        ptr_type
        get(context_t& context,
            const args_type& args)
        {
            return ptr_type(
                new T(
                    boost::ref(context),
                    boost::get<0>(args),
                    boost::get<1>(args),
                    boost::get<2>(args)
                )
            );
        }
    };
};

}} // namespace cocaine::api

#endif
