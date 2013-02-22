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
#include "cocaine/asio/readable_stream.hpp"
#include "cocaine/asio/writable_stream.hpp"
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
    message_t(msgpack::object object):
        m_object(object)
    { }

    template<class Event, typename... Args>
    void
    as(Args&&... targets) const {
        try {
            type_traits<typename event_traits<Event>::tuple_type>::unpack(
                args(),
                std::forward<Args>(targets)...
            );
        } catch(const msgpack::type_error&) {
            throw cocaine::error_t("invalid message type");
        }
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

private:
    msgpack::object m_object;
};

template<class Stream>
struct encoder:
    boost::noncopyable
{
    typedef writable_stream<Stream> stream_type;

    encoder():
        m_packer(m_buffer)
    { }

    template<class Event, typename... Args>
    void
    write(Args&&... args) {
        typedef event_traits<Event> traits;

        // NOTE: Format is [ID, [Args...]].
        m_packer.pack_array(2);

        type_traits<int>::pack(
            m_packer,
            static_cast<int>(traits::id)
        );

        type_traits<typename traits::tuple_type>::pack(
            m_packer,
            std::forward<Args>(args)...
        );

        std::unique_lock<std::mutex> lock(m_mutex);

        if(m_stream) {
            m_stream->write(m_buffer.data(), m_buffer.size());
        } else {
            m_cache.emplace_back(m_buffer.data(), m_buffer.size());
        }

        m_buffer.clear();
    }

    void
    attach(const std::shared_ptr<stream_type>& stream) {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_stream = stream;

        for(message_cache_t::const_iterator it = m_cache.begin();
            it != m_cache.end();
            ++it)
        {
            m_stream->write(it->data(), it->size());
        }
    }

public:
    std::shared_ptr<stream_type>
    stream() {
        return m_stream;
    }

private:
    msgpack::sbuffer m_buffer;
    msgpack::packer<msgpack::sbuffer> m_packer;

    typedef std::vector<
        std::string
    > message_cache_t;

    // Message cache.
    message_cache_t m_cache;
    std::mutex m_mutex;

    // Attachable stream.
    std::shared_ptr<stream_type> m_stream;
};

template<class Stream>
struct decoder:
    boost::noncopyable
{
    typedef readable_stream<Stream> stream_type;

    ~decoder() {
        unbind();
    }

    template<class Callback>
    void
    bind(Callback callback) {
        m_callback = callback;

        using namespace std::placeholders;

        m_stream->bind(
            std::bind(&decoder::on_event, this, _1, _2)
        );
    }

    void
    unbind() {
        m_callback = nullptr;
        m_stream->unbind();
    }

    void
    attach(const std::shared_ptr<stream_type>& stream) {
        m_stream = stream;
    }

public:
    std::shared_ptr<stream_type>
    stream() {
        return m_stream;
    }

private:
    size_t
    on_event(const char * data, size_t size) {
        size_t offset = 0,
               checkpoint = 0;

        msgpack::unpack_return rv;

        do {
            msgpack::object object;
            msgpack::zone zone;

            rv = msgpack::unpack(data, size, &offset, &zone, &object);

            switch(rv) {
                case msgpack::UNPACK_EXTRA_BYTES:
                case msgpack::UNPACK_SUCCESS:
                    m_callback(message_t(object));

                    if(rv == msgpack::UNPACK_SUCCESS) {
                        return size;
                    }

                    checkpoint = offset;

                    break;

                case msgpack::UNPACK_CONTINUE:
                    return checkpoint;

                case msgpack::UNPACK_PARSE_ERROR:
                    throw cocaine::error_t("corrupted message");
            }
        } while(true);
    }

private:
    std::function<
        void(const message_t&)
    > m_callback;

    // Attachable stream.
    std::shared_ptr<stream_type> m_stream;
};

template<class Stream>
struct codec {
    codec(service_t& service,
          const std::shared_ptr<Stream>& stream):
        rd(new decoder<Stream>()),
        wr(new encoder<Stream>())
    {
        rd->attach(std::make_shared<readable_stream<Stream>>(service, stream));
        wr->attach(std::make_shared<writable_stream<Stream>>(service, stream));
    }

    std::unique_ptr<decoder<Stream>> rd;
    std::unique_ptr<encoder<Stream>> wr;
};

}} // namespace cocaine::io

#endif
