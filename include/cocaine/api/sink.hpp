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

#ifndef COCAINE_SINK_API_HPP
#define COCAINE_SINK_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace api {

static inline
logging::priorities
resolve(const Json::Value& args) {
    const std::string& priority = args["verbosity"].asString();

    if(priority == "ignore") {
        return logging::ignore;
    } else if(priority == "debug") {
        return logging::debug;
    } else if(priority == "warning") {
        return logging::warning;
    } else if(priority == "error") {
        return logging::error;
    } else {
        return logging::info;
    }
}

class sink_t:
    public boost::noncopyable
{
    public:
        virtual
        ~sink_t() { 
            // Empty.
        }

        virtual
        logging::priorities
        verbosity() const {
            return m_verbosity;
        }

        virtual
        void
        emit(logging::priorities priority,
             const std::string& source,
             const std::string& message) = 0;

    protected:
        sink_t(const std::string& /* name */,
               const Json::Value& args):
            m_verbosity(resolve(args))
        {
        }

    private:
        const logging::priorities m_verbosity;
};

template<>
struct category_traits<sink_t> {
    typedef std::unique_ptr<sink_t> ptr_type;

    struct factory_type:
        public factory_base<sink_t>
    {
        virtual
        ptr_type
        get(const std::string& name,
            const Json::Value& args) = 0;
    };

    template<class T>
    struct default_factory:
        public factory_type
    {
        virtual
        ptr_type
        get(const std::string& name,
            const Json::Value& args)
        {
            return ptr_type(new T(name, args));
        }
    };
};

}} // namespace cocaine::api

#endif
