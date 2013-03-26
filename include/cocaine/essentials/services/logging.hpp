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

#ifndef COCAINE_LOGGING_SERVICE_HPP
#define COCAINE_LOGGING_SERVICE_HPP

#include "cocaine/api/service.hpp"

namespace cocaine {

namespace io {

struct logging_tag;

namespace logging {
    struct emit {
        typedef logging_tag tag;

        typedef boost::mpl::list<
            /* level */   int,
            /* source */  std::string,
            /* message */ std::string
        > tuple_type;
    };
}

template<>
struct protocol<logging_tag> {
    typedef mpl::list<
        logging::emit
    > type;
};

} // namespace io

namespace service {

class logging_t:
    public api::service_t
{
    public:
        logging_t(context_t& context,
                  io::reactor_t& reactor,
                  const std::string& name,
                  const Json::Value& args);

    private:
        void
        on_emit(int priority,
                const std::string& source,
                const std::string& message);

    private:
        context_t& m_context;

#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            std::string,
            std::shared_ptr<logging::log_t>
        > log_map_t;

        log_map_t m_logs;
};

} // namespace service

} // namespace cocaine

#endif
