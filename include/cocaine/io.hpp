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
// ------------------------

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

// Customized type serialization
// -----------------------------

template<class T>
struct raw_traits;

template<>
struct raw_traits<std::string> {
    static
    void
    pack(zmq::message_t& message, const std::string& value) {
        message.rebuild(value.size());
        memcpy(message.data(), value.data(), value.size());
    }

    static
    bool
    unpack(zmq::message_t& message, std::string& value) {
        value.assign(
            static_cast<const char*>(message.data()),
            message.size()
        );

        return true;
    }
};

// ZeroMQ socket wrapper
// ---------------------

class socket_t: 
    public boost::noncopyable,
    public birth_control<socket_t>
{
    public:
        socket_t(context_t& context,
                 int type);

        socket_t(context_t& context,
                 int type,
                 const std::string& route);

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

            return raw_traits<
                typename boost::remove_const<T>::type
            >::unpack(message, result.value);
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
        route() {
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

            return (events & event) == event;
        }

    private:
        context_t& m_context;

        zmq::socket_t m_socket;
        std::string m_endpoint;
};

// Socket options
// --------------

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
// ---------------------------

using namespace boost::tuples;

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
// --------------------------

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
// -----------

template<class Event> 
struct message:
    public event_traits<Event>::tuple_type
{
    template<typename... Args>
    message(Args&&... args):
        event_traits<Event>::tuple_type(args...)
    { }
};

// Unknown message proxy
/* ---------------------

struct unknown;

template<>
struct message<unknown> {
    message() { }
    
    message(const std::string& route, int type):
        route_(route),
        type_(type)
    { }

    ~message() {
        for(std::deque<zmq::message_t*>::iterator it = chain_.begin();
            it != chain_.end();
            ++it)
        {
            delete *it;
        }
    }

    int
    type() const {
        return type_;
    }

    std::string
    route() const {
        return route_;
    }

    template<class Event>
    void 
    as(message<Event>& message) {
        BOOST_ASSERT(get<Event>::value == type());
        unpack(message);
    }

    void
    push(zmq::message_t * part) {
        chain_.push_back(part);
    }

private:
    void
    unpack(const null_type&) const {
        return;
    }

    template<class Head, class Tail>
    void
    unpack(cons<Head, Tail>& o) {
        msgpack::unpacked unpacked;

        msgpack::unpack(
            &unpacked,
            static_cast<const char*>(chain_.front()->data()),
            chain_.front()->size()
        );

        type_traits<Head>::unpack(unpacked.get(), o.get_head());
        std::rotate(chain_.begin(), chain_.begin() + 1, chain_.end());        

        return unpack(o.get_tail());
    }

    template<class Tail>
    void
    unpack(cons<zmq::message_t&, Tail>& o) {
        o.get_head().move(chain_.front());

        std::rotate(chain_.begin(), chain_.begin() + 1, chain_.end());        

        return unpack(o.get_tail());
    }

private:
    std::string route_;
    int type_;
    std::deque<zmq::message_t*> chain_;
};

*/

// RPC channel
// -----------

class channel_t:
    public socket_t
{
    public:
        channel_t(context_t& context, const std::string& route):
            socket_t(context, ZMQ_ROUTER, route)
        { }

        using socket_t::send;
        using socket_t::recv;

        // Sending
        // -------

        template<class Event>
        bool
        send(const std::string& route,
             const message<Event>& object)
        {
            const bool multipart = boost::tuples::length<
                typename event_traits<Event>::tuple_type
            >::value;

            return send(protect(route), ZMQ_SNDMORE) &&
                   send(get<Event>::value, multipart ? ZMQ_SNDMORE : 0) &&
                   send_tuple(object);
        }

        // Receiving
        /* ---------

        message<unknown>
        recv(int flags = 0) {
            std::string route;
            raw<std::string> raw(route);
            int type;

            if(!recv(raw, flags)) {
                // Do something.
            }

            if(!recv(type, flags)) {
                // Do something.
            }

            message<unknown> result(route, type);

            while(more()) {
                zmq::message_t * part = new zmq::message_t();
                recv(part);
                result.push(part);
            }

            return result;
        }

        */

        bool
        recv_multi(const null_type&,
                   int __attribute__ ((unused)) flags = 0) const
        {
            return true;
        }

        template<class Head>
        bool
        recv_multi(cons<Head, null_type>& o,
                   int flags = 0)
        {
            return recv(o.get_head(), flags);
        }

        template<class Head, class Tail>
        bool
        recv_multi(cons<Head, Tail>& o,
                   int flags = 0)
        {
            if(!recv(o.get_head(), flags)) {
                return false;
            } else if(more()) {
                return recv_multi(o.get_tail(), flags);
            } else {
                throw std::runtime_error("corrupted object - misplaced chunks");
            }
        }

    private:
        bool
        send_tuple(const null_type&,
                   int __attribute__ ((unused)) flags = 0) const
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
};

}} // namespace cocaine::io

#endif
