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
#include "cocaine/channel.hpp"

namespace cocaine { namespace api {

struct client_t:
    public boost::noncopyable
{
    client_t(context_t& context,
             const std::string& name,
#if ZMQ_VERSION > 30200
             int watermark
#else
             uint64_t watermark
#endif
    ):
        m_channel(context, ZMQ_DEALER)
    {
        std::string endpoint = cocaine::format(
            "ipc://%1%/services/%2%",
            context.config.path.runtime,
            name
        );

        // Set the channel high watermark, so that if the service goes down
        // it could be determined by the client within some reasonable timespan.
#if ZMQ_VERSION > 30200
        m_channel.setsockopt(ZMQ_SNDHWM, &watermark, sizeof(watermark));
#else
        m_channel.setsockopt(ZMQ_HWM, &watermark, sizeof(watermark));
#endif

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
    bool
    send(Args&&... args) {
        boost::unique_lock<io::shared_channel_t> lock(m_channel);

        io::scoped_option<
            io::options::send_timeout
        > option(m_channel, 0);

        return m_channel.send(m_codec.pack<Event>(std::forward<Args>(args)...));
    }

private:
    io::shared_channel_t m_channel;
    io::codec_t m_codec;
};

}} // namespace cocaine::api

#endif
