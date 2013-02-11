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

#ifndef COCAINE_CLIENT_API_HPP
#define COCAINE_CLIENT_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/context.hpp"
#include "cocaine/messaging.hpp"

#include "cocaine/asio/pipe.hpp"

namespace cocaine { namespace api {

struct client_t:
    boost::noncopyable
{
    client_t(context_t& context,
             const std::string& name)
    {
        std::string endpoint = cocaine::format(
            "%1%/services/%2%",
            context.config.path.runtime,
            name
        );

        m_encoder.attach(std::make_shared<io::writable_stream<io::pipe_t>>(
            m_service,
            std::make_shared<io::pipe_t>(endpoint)
        ));
    }

    template<class Event, typename... Args>
    bool
    send(Args&&... args) {
        m_encoder.write<Event>(std::forward<Args>(args)...);
        return true;
    }

private:
    io::service_t m_service;
    io::encoder<io::pipe_t> m_encoder;
};

}} // namespace cocaine::api

#endif
