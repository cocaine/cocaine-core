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

struct message_t {
    message_t():
        m_id(-1)
    { }

    message_t(msgpack::unpacked&& u):
        m_object(u.get()),
        m_zone(std::move(u.zone()))
    {
        m_object.via.array.ptr[0] >> m_id;
    }

    message_t(message_t&& other) {
        *this = std::move(other);
    }
    
    message_t&
    operator=(message_t&& other) {
        m_id = other.m_id;
        m_object = other.m_object;
        m_zone = std::move(other.m_zone);

        return *this;
    }

    template<class Event, typename... Args>
    void
    as(Args&&... args) {
        try {
            type_traits<typename event_traits<Event>::tuple_type>::unpack(
                m_object,
                std::forward<Args>(args)...
            );
        } catch(const msgpack::type_error& e) {
            throw cocaine::error_t("message type mismatch");
        } catch(const std::bad_cast& e) {
            throw cocaine::error_t("message type mismatch");
        }
    }

public:
    int
    id() const {
        return m_id;
    }

private:
    int m_id;

    msgpack::object m_object;
    std::unique_ptr<msgpack::zone> m_zone;
};

struct codec_t {
    codec_t(size_t size = 1024):
        m_buffer(size),
        m_packer(m_buffer)
    { }

    template<class Event, typename... Args>
    std::string
    pack(Args&&... args) {
        m_buffer.clear();
        m_packer.pack_array(2);

        // Pack the event code.
        type_traits<int>::pack(
            m_packer,
            event_traits<Event>::id
        );

        // Pack the event data.
        type_traits<typename event_traits<Event>::tuple_type>::pack(
            m_packer,
            std::forward<Args>(args)...
        );

        return std::string(m_buffer.data(), m_buffer.size());
    }

    message_t
    unpack(const std::string& blob) {
        msgpack::unpacked unpacked;

        try {
            msgpack::unpack(&unpacked, blob.data(), blob.size());
        } catch(const msgpack::unpack_error& e) {
            throw cocaine::error_t("corrupted message");
        }

        msgpack::object object = unpacked.get();

        if(object.type != msgpack::type::ARRAY ||
           object.via.array.size != 2)
        {
            throw cocaine::error_t("invalid message format");
        }

        return message_t(std::move(unpacked));
    }

private:
    msgpack::sbuffer m_buffer;
    msgpack::packer<msgpack::sbuffer> m_packer;
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
};

}} // namespace cocaine::io

#endif
