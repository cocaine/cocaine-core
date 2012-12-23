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

#ifndef COCAINE_CHANNEL_HPP
#define COCAINE_CHANNEL_HPP

#include "cocaine/io.hpp"

#include <boost/mpl/begin.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/distance.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/size.hpp>

namespace cocaine { namespace io {

namespace mpl = boost::mpl;

template<class Tag>
struct protocol;

namespace detail {
    template<
        class Event,
        class Protocol = typename protocol<
            typename Event::tag
        >::type
    >
    struct enumerate:
        public mpl::distance<
            typename mpl::begin<Protocol>::type,
            typename mpl::find<Protocol, Event>::type
        >::type
    {
        static_assert(
            mpl::contains<Protocol, Event>::value,
            "event has not been registered with its protocol"
        );
    };

    template<class T>
    struct depend {
        typedef void type;
    };

    template<class Event, class U = void>
    struct tuple_type {
        typedef mpl::list<> type;
    };

    template<class Event>
    struct tuple_type<
        Event,
        typename depend<typename Event::tuple_type>::type
    > 
    {
        typedef typename Event::tuple_type type;
    };
}

template<class Event>
struct event_traits {
    typedef typename detail::tuple_type<
        Event
    >::type tuple_type;

    enum constants {
        id = detail::enumerate<Event>::value,
        length = mpl::size<tuple_type>::value,
        empty = length == 0
    };
};

template<class SharingPolicy>
class channel:
    public socket<SharingPolicy>
{
    public:
        channel(context_t& context, int type):
            socket<SharingPolicy>(context, type)
        { }
        
        template<class T>
        channel(context_t& context, int type, const T& identity):
            socket<SharingPolicy>(context, type)
        {
            msgpack::sbuffer buffer;
            msgpack::packer<msgpack::sbuffer> packer(buffer);

            // NOTE: Channels allow any serializable type to be its
            // identity, for example UUIDs.
            type_traits<T>::pack(packer, identity);
            
            this->setsockopt(ZMQ_IDENTITY, buffer.data(), buffer.size());
        }

        using socket<SharingPolicy>::send;
        using socket<SharingPolicy>::recv;

        // RPC messages

        template<class Event, class T, typename... Args>
        bool
        send(T&& head, Args&&... tail) {
            msgpack::sbuffer buffer;

            type_traits<typename event_traits<Event>::tuple_type>::pack(
                buffer,
                std::forward<T>(head),
                std::forward<Args>(tail)...
            );

            zmq::message_t message(buffer.size());

            std::memcpy(
                message.data(),
                buffer.data(),
                buffer.size()
            );

            return this->send_multipart(
                static_cast<int>(event_traits<Event>::id),
                message
            );
        }

        template<class Event>
        bool
        send() {
            return this->send(
                static_cast<int>(event_traits<Event>::id)
            );
        }

        template<class Event, class T, typename... Args>
        bool
        recv(T&& head, Args&&... tail) {
            zmq::message_t message;
            msgpack::unpacked unpacked;

            if(!this->recv(message)) {
                return false;
            }

            try {
                msgpack::unpack(
                    &unpacked,
                    static_cast<const char*>(message.data()),
                    message.size()
                );
            } catch(const msgpack::unpack_error& e) {
                throw cocaine::error_t("corrupted message");
            }

            try {
                type_traits<typename event_traits<Event>::tuple_type>::unpack(
                    unpacked.get(),
                    std::forward<T>(head),
                    std::forward<Args>(tail)...
                );
            } catch(const msgpack::type_error& e) {
                throw cocaine::error_t("message type mismatch");
            } catch(const std::bad_cast& e) {
                throw cocaine::error_t("message type mismatch");
            }

            return true;
        }

        // XXX: This has to go.

        bool
        send_message(int message_id,
                     const std::string& message)
        {
            return this->send(message_id, message.size() ? ZMQ_SNDMORE : 0) &&
                   (!message.size() || this->send(protect(message)));
        }
};

}} // namespace cocaine::io

#endif
