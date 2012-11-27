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
#include "cocaine/io.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/helpers/json.hpp"

namespace cocaine { namespace api {

template<class Tag>
class client:
    public boost::noncopyable
{
    public:
        client(context_t& context,
               const std::string& name):
            m_channel(context, ZMQ_DEALER)
        {
            std::string endpoint = cocaine::format(
                "ipc://%1%/services/%2%",
                context.config.ipc_path,
                name
            );

            try {
                m_channel.connect(endpoint);
            } catch(const zmq::error_t& e) {
                throw configuration_error_t(
                    "unable to connect to the '%s' service channel - %s",
                    name,
                    e.what()
                );
            }
        }

        template<class Event, typename... Args>
        void
        send(Args&&... args) {
            m_channel.template send<Event>(std::forward<Args>(args)...);
        }

    private:
        typedef io::channel<
            Tag,
            io::policies::unique
        > rpc_channel_t;

        rpc_channel_t m_channel;
};

}} // namespace cocaine::api

#endif
