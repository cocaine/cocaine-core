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

#include <boost/function.hpp>

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"
#include "cocaine/unique_id.hpp"

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

typedef boost::function<
    void(const std::string&)
> chunk_fn_t;

typedef boost::function<
    void()
> close_fn_t;

struct request_t {
    request_t(const unique_id_t& id_):
        id(id_)
    { }

    virtual
    void
    on_chunk(chunk_fn_t callback) = 0;

    virtual
    void
    on_close(close_fn_t callback) = 0;

public:
    const unique_id_t id;
};

struct response_t {
    response_t(const unique_id_t& id_):
        id(id_)
    { }

    virtual
    void
    write(const void * chunk,
          size_t size) = 0;

    virtual
    void
    close() = 0;

public:
    const unique_id_t id;
};

typedef boost::function<
    void(const boost::shared_ptr<request_t>&, const boost::shared_ptr<response_t>&)
> event_fn_t;

struct emitter_t {
    virtual
    void
    on_event(const std::string& event,
             event_fn_t callback) = 0;
};

class sandbox_t:
    public boost::noncopyable
{
    public:
        virtual
        ~sandbox_t() {
            // Empty.
        }
        
    protected:
        sandbox_t(context_t&,
                  const std::string& /* name */,
                  const Json::Value& /* args */,
                  const std::string& /* spool */,
                  emitter_t&)
        {
           // Empty. 
        }
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
            const std::string& spool,
            emitter_t& emitter) = 0;
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
            const std::string& spool,
            emitter_t& emitter)
        {
            return ptr_type(
                new T(context, name, args, spool, emitter)
            );
        }
    };
};

}} // namespace cocaine::api

#endif
