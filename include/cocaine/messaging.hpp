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

#ifndef COCAINE_MESSAGING_HPP
#define COCAINE_MESSAGING_HPP

#include "cocaine/common.hpp"
#include "cocaine/traits.hpp"

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

struct message_t:
    boost::noncopyable
{
    message_t() {
        // Empty.
    }

    message_t(msgpack::unpacked&& unpacked) {
        m_object = unpacked.get();
        m_zone = std::move(unpacked.zone());
    }

    message_t(message_t&& other) {
        *this = std::move(other);
    }

    message_t&
    operator=(message_t&& other) {
        m_object = other.m_object;
        m_zone = std::move(other.m_zone);

        return *this;
    }

public:
    int
    id() const {
        return m_object.via.array.ptr[0].as<int>();
    }

    const msgpack::object&
    args() const {
        return m_object.via.array.ptr[1];
    }

public:
    template<class Event, typename... Args>
    void
    as(Args&&... targets) {
        try {
            type_traits<typename event_traits<Event>::tuple_type>::unpack(
                args(),
                std::forward<Args>(targets)...
            );
        } catch(const msgpack::type_error&) {
            throw cocaine::error_t("invalid message type");
        }
    }

private:
    msgpack::object m_object;
    std::unique_ptr<msgpack::zone> m_zone;
};

struct codec {
    template<class Event, typename... Args>
    static inline
    std::string
    pack(Args&&... args) {
        msgpack::sbuffer buffer;
        msgpack::packer<msgpack::sbuffer> packer(buffer);

        packer.pack_array(2);

        type_traits<int>::pack(
            packer,
            event_traits<Event>::id
        );

        if(!event_traits<Event>::empty) {
            type_traits<typename event_traits<Event>::tuple_type>::pack(
                packer,
                std::forward<Args>(args)...
            );
        } else {
            packer.pack_nil();
        }

        return std::string(buffer.data(), buffer.size());
    }

    static inline
    message_t
    unpack(const std::string& blob) {
        msgpack::unpacked unpacked;

        try {
            msgpack::unpack(&unpacked, blob.data(), blob.size());
        } catch(const msgpack::unpack_error& e) {
            throw cocaine::error_t("corrupted message");
        }

        const msgpack::object& object = unpacked.get();

        if(object.type != msgpack::type::ARRAY ||
           object.via.array.size != 2)
        {
            throw cocaine::error_t("invalid message format");
        }

        return message_t(std::move(unpacked));
    }
};

}} // namespace cocaine::io

#endif
