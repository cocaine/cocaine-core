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

#ifndef COCAINE_LOGGER_API_HPP
#define COCAINE_LOGGER_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/json.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/repository.hpp"

namespace cocaine { namespace api {

static inline
logging::priorities
resolve(const Json::Value& args) {
    const std::string& verbosity = args["verbosity"].asString();

    if(verbosity == "ignore") {
        return logging::ignore;
    } else if(verbosity == "debug") {
        return logging::debug;
    } else if(verbosity == "warning") {
        return logging::warning;
    } else if(verbosity == "error") {
        return logging::error;
    } else {
        return logging::info;
    }
}

class logger_t:
    public logging::logger_t
{
    public:
        virtual
        ~logger_t() {
            // Empty.
        }

        virtual
        logging::priorities
        verbosity() const {
            return m_verbosity;
        }

    protected:
        logger_t(context_t&,
                 const std::string& /* name */,
                 const Json::Value& args):
            m_verbosity(resolve(args))
        { }

    private:
        const logging::priorities m_verbosity;
};

template<>
struct category_traits<logger_t> {
    typedef std::unique_ptr<logger_t> ptr_type;

    struct factory_type:
        public factory_base<logger_t>
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
            return ptr_type(new T(context, name, args));
        }
    };
};

category_traits<logger_t>::ptr_type
logger(context_t& context,
       const std::string& name);

}} // namespace cocaine::api

#endif
