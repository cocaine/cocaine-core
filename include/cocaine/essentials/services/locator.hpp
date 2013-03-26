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

#ifndef COCAINE_LOCATOR_SERVICE_HPP
#define COCAINE_LOCATOR_SERVICE_HPP

#include "cocaine/api/service.hpp"

namespace cocaine {

namespace service {

struct description_t {
    // An endpoint for the client to connect to in order to use the service.
    std::string endpoint;

    // Service protocol version.
    unsigned int version;

    // A mapping between method slot numbers and names for use in dynamic
    // languages like Python or Ruby.
    std::map<int, std::string> methods;

    MSGPACK_DEFINE(endpoint, version, methods);
};

class locator_t:
    public api::service_t
{
    public:
        locator_t(context_t& context,
                  io::reactor_t& reactor,
                  const std::string& name,
                  const Json::Value& args);

    private:
        description_t
        resolve(const std::string& name) const;
};

} // namespace service

namespace io {

struct locator_tag;

namespace locator {
    struct resolve {
        typedef locator_tag tag;

        typedef boost::mpl::list<
            /* service */ std::string
        > tuple_type;
    };
}

template<>
struct protocol<locator_tag> {
    typedef mpl::list<
        locator::resolve
    > type;
};

} // namespace io

} // namespace cocaine

#endif
