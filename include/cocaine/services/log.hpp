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

#ifndef COCAINE_LOG_SERVICE_HPP
#define COCAINE_LOG_SERVICE_HPP

#include "cocaine/services/basic.hpp"

#include <boost/bind.hpp>

namespace cocaine {

struct log_tag;

struct message {
    typedef log_tag tag;

    typedef boost::mpl::list<
        int,
        std::string
    > tuple_type;
};

namespace io {
    template<>
    struct dispatch<log_tag> {
        typedef mpl::list<
            message
        > category;
    };
}

namespace service {

class log_t:
    public api::basic_service<log_tag>
{
    public:
        log_t(context_t& context,
              const std::string& name,
              const Json::Value& args):
            api::basic_service<log_tag>(context, name, args)
        {
            on<message>(boost::bind(&log_t::on_message, this, _1, _2));
        }

        void
        on_message(int priority,
                   std::string message)
        {
            COCAINE_LOG(
               log(),
               static_cast<logging::priorities>(priority),
               "%s",
               message
            );
        }
};

}}

#endif
