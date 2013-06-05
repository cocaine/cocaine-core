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

#ifndef COCAINE_LOGGER_API_HPP
#define COCAINE_LOGGER_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/repository.hpp"

#include "json/json.h"

namespace cocaine { namespace api {

class logger_t:
    public logging::logger_concept_t
{
    public:
        typedef logger_t category_type;

    public:
        virtual
        logging::priorities
        verbosity() const {
            return m_verbosity;
        }

    protected:
        logger_t(const Json::Value& args):
            m_verbosity(resolve(args))
        { }

    private:
        static
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

    private:
        const logging::priorities m_verbosity;
};

template<>
struct category_traits<logger_t> {
    typedef std::unique_ptr<logger_t> ptr_type;

    struct factory_type:
        public basic_factory<logger_t>
    {
        virtual
        ptr_type
        get(const Json::Value& args) = 0;
    };

    template<class T>
    struct default_factory:
        public factory_type
    {
        virtual
        ptr_type
        get(const Json::Value& args) {
            return ptr_type(new T(args));
        }
    };
};

}} // namespace cocaine::api

#endif
