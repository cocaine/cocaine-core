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

#ifndef COCAINE_IO_HPP
#define COCAINE_IO_HPP

#include <boost/mpl/begin.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/distance.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/list.hpp>

#include <boost/tuple/tuple.hpp>
#include <boost/type_traits/remove_const.hpp>

#include <zmq.hpp>

#if ZMQ_VERSION < 20200
    #error ZeroMQ version 2.2.0+ required!
#endif

#include "cocaine/common.hpp"
#include "cocaine/traits.hpp"

#include "cocaine/helpers/birth_control.hpp"

namespace cocaine { namespace io {

// Custom message container

template<class T>
struct raw {
    raw(T& value_):
        value(value_)
    { }

    T& value;
};

template<class T>
static inline
raw<T>
protect(T& object) {
    return raw<T>(object);
}

template<class T>
static inline
raw<const T>
protect(const T& object) {
    return raw<const T>(object);
}

// Customized type serialization

template<class T>
struct raw_traits;

template<>
struct raw_traits<std::string> {
    static
    void
    pack(zmq::message_t& message,
         const std::string& value)
    {
        message.rebuild(value.size());
        memcpy(message.data(), value.data(), value.size());
    }

    static
    void
    unpack(zmq::message_t& message,
           std::string& value)
    {
        value.assign(
            static_cast<const char*>(message.data()),
            message.size()
        );
    }
};

// ZeroMQ socket wrapper

using namespace boost::tuples;

class socket_t: 
    public boost::noncopyable,
    public birth_control<socket_t>
{
    public:
        socket_t(context_t& context,
                 int type);

        void
        bind(const std::string& endpoint);
        
        void
        connect(const std::string& endpoint);
       
        bool
        send(zmq::message_t& message,
             int flags = 0)
        {
            return m_socket.send(message, flags);
        }

        template<class T>
        bool
        send(const T& value,
             int flags = 0)
        {
            msgpack::sbuffer buffer;
            msgpack::packer<msgpack::sbuffer> packer(buffer);

            type_traits<T>::pack(packer, value);
            
            zmq::message_t message(buffer.size());
            memcpy(message.data(), buffer.data(), buffer.size());
            
            return send(message, flags);
        }
        
        template<class T>
        bool
        send(const raw<T>& object,
             int flags = 0)
        {
            zmq::message_t message;
            
            raw_traits<
                typename boost::remove_const<T>::type
            >::pack(message, object.value);
            
            return send(message, flags);
        }

        bool
        send_tuple(const null_type&,
                   int __attribute__((unused)) flags = 0) const
        {
            return true;
        }
        
        template<class Head>
        bool
        send_tuple(const cons<Head, null_type>& o,
                   int flags = 0)
        {
            return send(o.get_head(), flags);
        }

        template<class Head, class Tail>
        bool
        send_tuple(const cons<Head, Tail>& o,
                   int flags = 0)
        {
            return send(o.get_head(), ZMQ_SNDMORE | flags) &&
                   send_tuple(o.get_tail(), flags);
        }
        bool
        recv(zmq::message_t * message,
             int flags = 0)
        {
            return m_socket.recv(message, flags);
        }

        template<class T>
        bool
        recv(T& result,
             int flags = 0)
        {
            zmq::message_t message;
            msgpack::unpacked unpacked;

            if(!recv(&message, flags)) {
                return false;
            }
           
            try { 
                msgpack::unpack(
                    &unpacked,
                    static_cast<const char*>(message.data()),
                    message.size()
                );
               
                type_traits<T>::unpack(unpacked.get(), result);
            } catch(const msgpack::type_error& e) {
                throw std::runtime_error("corrupted object");
            } catch(const std::bad_cast& e) {
                throw std::runtime_error("corrupted object - type mismatch");
            }

            return true;
        }
      
        template<class T>
        bool
        recv(raw<T>& result,
             int flags = 0)
        {
            zmq::message_t message;

            if(!recv(&message, flags)) {
                return false;
            }

            raw_traits<
                typename boost::remove_const<T>::type
            >::unpack(message, result.value);
        
            return true;
        }

        bool
        recv_tuple(const null_type&,
                   int __attribute__((unused)) flags = 0) const
        {
            return true;
        }

        template<class Head>
        bool
        recv_tuple(cons<Head, null_type>&& o,
                   int flags = 0)
        {
            return recv(o.get_head(), flags);
        }

        template<class Head, class Tail>
        bool
        recv_tuple(cons<Head, Tail>&& o,
                   int flags = 0)
        {
            if(!recv(o.get_head(), flags)) {
                return false;
            } else {
                return recv_tuple(o.get_tail(), flags | ZMQ_NOBLOCK);
            }
        }

        void
        getsockopt(int name,
                   void * value,
                   size_t * size)
        {
            m_socket.getsockopt(name, value, size);
        }

        void
        setsockopt(int name,
                   const void * value,
                   size_t size)
        {
            m_socket.setsockopt(name, value, size);
        }

        void
        drop();

    public:
        std::string
        endpoint() const { 
            return m_endpoint; 
        }

        bool
        more() {
            int64_t rcvmore = 0;
            size_t size = sizeof(rcvmore);

            getsockopt(ZMQ_RCVMORE, &rcvmore, &size);

            return rcvmore != 0;
        }

        std::string
        identity() {
            char identity[256] = { 0 };
            size_t size = sizeof(identity);

            getsockopt(ZMQ_IDENTITY, &identity, &size);

            return identity;
        }

        int
        fd() {
            int fd = 0;
            size_t size = sizeof(fd);

            getsockopt(ZMQ_FD, &fd, &size);

            return fd;
        }

        bool
        pending(unsigned long event = ZMQ_POLLIN) {
            unsigned long events = 0;
            size_t size = sizeof(events);

            getsockopt(ZMQ_EVENTS, &events, &size);

            return events & event;
        }

    private:
        context_t& m_context;

        zmq::socket_t m_socket;
        std::string m_endpoint;
};

// Socket options

namespace options {
    struct receive_timeout {
        typedef int value_type;
        typedef boost::mpl::int_<ZMQ_RCVTIMEO> option_type;
    };

    struct send_timeout {
        typedef int value_type;
        typedef boost::mpl::int_<ZMQ_SNDTIMEO> option_type;
    };
}

template<class Option>
class scoped_option {
    typedef typename Option::value_type value_type;
    typedef typename Option::option_type option_type;

    public:
        scoped_option(socket_t& socket_, value_type value):
            socket(socket_),
            saved(value_type()),
            size(sizeof(saved))
        {
            socket.getsockopt(option_type(), &saved, &size);
            socket.setsockopt(option_type(), &value, sizeof(value));
        }

        ~scoped_option() {
            socket.setsockopt(option_type(), &saved, sizeof(saved));
        }

    private:
        socket_t& socket;
        
        value_type saved;
        size_t size;
};

// Event tuple type extraction

template<class T>
struct depend {
    typedef void type;
};

template<class Event, class Tuple = void>
struct event_traits {
    typedef boost::tuple<> tuple_type;
};

template<class Event>
struct event_traits<
    Event,
    typename depend<
        typename Event::tuple_type
    >::type
>
{
    typedef typename Event::tuple_type tuple_type;
};

// Event category enumeration

template<class Tag>
struct dispatch;

template<
    class Event,
    class Category = typename dispatch<
        typename Event::tag
    >::category
>
struct get:
    public boost::mpl::distance<
        typename boost::mpl::begin<Category>::type,
        typename boost::mpl::find<Category, Event>::type
    >::type
{
    // TEST: Event should be enumerated.
    BOOST_MPL_ASSERT(( boost::mpl::contains<Category, Event> ));
};

// RPC message

template<class Event> 
struct message:
    public event_traits<Event>::tuple_type
{
    template<typename... Args>
    message(Args&&... args):
        event_traits<Event>::tuple_type(std::forward<Args>(args)...)
    { }
};

// RPC channel

class channel_t:
    public socket_t
{
    public:
        channel_t(context_t& context, int type):
            socket_t(context, type)
        { }
        
        template<class T>
        channel_t(context_t& context, int type, const T& identity):
            socket_t(context, type)
        {
            msgpack::sbuffer buffer;
            msgpack::packer<msgpack::sbuffer> packer(buffer);

            // NOTE: Channels allow any serializable type to be its
            // identity, for example UUIDs.
            type_traits<T>::pack(packer, identity);
            
            setsockopt(ZMQ_IDENTITY, buffer.data(), buffer.size());
        }

        using socket_t::send;
        using socket_t::recv;

        // Sending

        template<class Event>
        bool
        send_message(const message<Event>& object) {
            const bool multipart = boost::tuples::length<
                typename event_traits<Event>::tuple_type
            >::value;

            return send(get<Event>::value, multipart ? ZMQ_SNDMORE : 0) &&
                   send_tuple(object);
        }
};

}} // namespace cocaine::io

#endif
