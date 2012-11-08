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

#include <boost/tuple/tuple.hpp>

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine {

struct unrecoverable_error_t:
    public error_t
{
    template<typename... Args>
    unrecoverable_error_t(const std::string& format,
                          const Args&... args):
        error_t(format, args...)
    { }
};

struct recoverable_error_t:
    public error_t
{
    template<typename... Args>
    recoverable_error_t(const std::string& format,
                        const Args&... args):
        error_t(format, args...)
    { }
};

namespace api {

struct io_t {
    virtual
    std::string
    read(int timeout) = 0;

    virtual
    void
    write(const void * data,
          size_t size) = 0;
};

class sandbox_t:
    public boost::noncopyable
{
    public:
        virtual
        ~sandbox_t() {
            // Empty.
        }
        
        virtual
        void
        invoke(const std::string& event,
               io_t& io) = 0;

    protected:
        sandbox_t(context_t&,
                  const std::string& /* name */,
                  const Json::Value& /* args */,
                  const std::string& /* spool */)
        {
           // Empty. 
        }
};

template<>
struct category_traits<api::sandbox_t> {
    typedef std::unique_ptr<api::sandbox_t> ptr_type;

    typedef boost::tuple<
        const std::string&,
        const Json::Value&,
        const std::string&
    > args_type;

    template<class T>
    struct default_factory:
        public factory<api::sandbox_t>
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
