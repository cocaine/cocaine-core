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

#include <boost/thread/mutex.hpp>
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

// Custom type serialization mechanics

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

// Socket sharing policies

namespace policies {
    struct unique {
        typedef struct {
            void lock() { };
            void unlock() { };
        } mutex_type;
    };

    struct shared {
        typedef boost::mutex mutex_type;
    };
}

// ZeroMQ socket wrapper

class socket_base_t: 
    public boost::noncopyable,
    public birth_control<socket_base_t>
{
    public:
        socket_base_t(context_t& context,
                      int type);

        ~socket_base_t();

        void
        bind();

        void
        bind(const std::string& endpoint);
        
        void
        connect(const std::string& endpoint);

        int
        fd() const {
            return m_fd;
        }

        std::string
        endpoint() const { 
            return m_endpoint; 
        }

    protected:
        zmq::socket_t m_socket;
        
    private:
        context_t& m_context;

        int m_fd;
        std::string m_endpoint;

        uint16_t m_port;
};

using namespace boost::tuples;

#define COCAINE_EINTR_GUARD(command)        \
    while(true) {                           \
        try {                               \
            command;                        \
        } catch(const zmq::error_t& e) {    \
            if(e.num() != EINTR) {          \
                throw;                      \
            }                               \
        }                                   \
    }

template<class SharingPolicy>
class socket:
    public socket_base_t
{
    public:
        socket(context_t& context,
               int type):
            socket_base_t(context, type)
        { }

        bool
        send(zmq::message_t& message,
             int flags = 0)
        {
            COCAINE_EINTR_GUARD(
                return m_socket.send(message, flags)
            );
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
            
            memcpy(
                message.data(),
                buffer.data(),
                buffer.size()
            );
            
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
        send_multipart(const null_type&,
                       int __attribute__((unused)) flags = 0) const
        {
            return true;
        }
        
        template<class Head>
        bool
        send_multipart(const cons<Head, null_type>& o,
                       int flags = 0)
        {
            return send(o.get_head(), flags);
        }

        template<class Head, class Tail>
        bool
        send_multipart(const cons<Head, Tail>& o,
                       int flags = 0)
        {
            return send(o.get_head(), ZMQ_SNDMORE | flags) &&
                   send_multipart(o.get_tail(), flags);
        }
        
        bool
        recv(zmq::message_t& message,
             int flags = 0)
        {
            COCAINE_EINTR_GUARD(
                return m_socket.recv(&message, flags)
            );
        }

        template<class T>
        bool
        recv(T& result,
             int flags = 0)
        {
            zmq::message_t message;
            msgpack::unpacked unpacked;

            if(!recv(message, flags)) {
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
                throw error_t("corrupted object");
            } catch(const std::bad_cast& e) {
                throw error_t("corrupted object - type mismatch");
            }

            return true;
        }

        template<class T>
        bool
        recv(raw<T>& result,
             int flags = 0)
        {
            zmq::message_t message;

            if(!recv(message, flags)) {
                return false;
            }

            raw_traits<
                typename boost::remove_const<T>::type
            >::unpack(message, result.value);

            return true;
        }

        template<class T>
        bool
        recv(raw<T>&& result,
             int flags = 0)
        {
            return recv(result, flags);
        }

        bool
        recv_multipart(const null_type&,
                       int __attribute__((unused)) flags = 0) const
        {
            return true;
        }

        template<class Head, class Tail>
        bool
        recv_multipart(cons<Head, Tail>& o,
                       int flags = 0)
        {
            return recv(o.get_head(), flags) &&
                   recv_multipart(o.get_tail(), flags | ZMQ_NOBLOCK);
        }

        template<class Head, class Tail>
        bool
        recv_multipart(cons<Head, Tail>&& o,
                       int flags = 0)
        {
            return recv(o.get_head(), flags) &&
                   recv_multipart(std::move(o.get_tail()), flags | ZMQ_NOBLOCK);
        }
        void
        getsockopt(int name,
                   void * value,
                   size_t * size)
        {
            COCAINE_EINTR_GUARD(
                return m_socket.getsockopt(name, value, size)
            );
        }

        void
        setsockopt(int name,
                   const void * value,
                   size_t size)
        {
            COCAINE_EINTR_GUARD(
                return m_socket.setsockopt(name, value, size)
            );
        }

        void
        drop() {
            zmq::message_t null;

            while(more()) {
                recv(null);
            }
        }

        // Lockable concept implementation

        void lock() {
            m_mutex.lock();
        }

        void unlock() {
            m_mutex.unlock();
        }

    public:
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

        bool
        pending(unsigned long event = ZMQ_POLLIN) {
            unsigned long events = 0;
            size_t size = sizeof(events);

            getsockopt(ZMQ_EVENTS, &events, &size);

            return events & event;
        }

    private:
        typename SharingPolicy::mutex_type m_mutex;
};

#undef COCAINE_EINTR_GUARD

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

template<class Option, class SharingPolicy>
class scoped_option {
    typedef typename Option::value_type value_type;
    typedef typename Option::option_type option_type;

    public:
        scoped_option(socket<SharingPolicy>& socket,
                      value_type value):
            target(socket),
            saved(value_type()),
            size(sizeof(saved))
        {
            target.getsockopt(option_type(), &saved, &size);
            target.setsockopt(option_type(), &value, sizeof(value));
        }

        ~scoped_option() {
            target.setsockopt(option_type(), &saved, sizeof(saved));
        }

    private:
        socket<SharingPolicy>& target;
        
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
    typedef tuple<> tuple_type;
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
struct enumerate:
    public boost::mpl::distance<
        typename boost::mpl::begin<Category>::type,
        typename boost::mpl::find<Category, Event>::type
    >::type
{
    static_assert(
        boost::mpl::contains<Category, Event>::value,
        "event has not been enumerated within its category"
    );
};

// RPC message

template<class Event> 
struct message:
    public event_traits<Event>::tuple_type,
    public enumerate<Event>
{
    template<typename... Args>
    message(Args&&... args):
        event_traits<Event>::tuple_type(std::forward<Args>(args)...)
    { }
};

// RPC channel

template<class Stream>
static inline
void
pack_sequence(msgpack::packer<Stream>&) {
    return;
}

template<class Stream, class Head, typename... Tail>
static inline
void
pack_sequence(msgpack::packer<Stream>& packer,
              Head&& head,
              Tail&&... tail)
{
    typedef typename boost::remove_const<
        typename boost::remove_reference<Head>::type
    >::type type;

    // Pack the current tuple element using the correct packer.
    type_traits<type>::pack(packer, head);

    // Recurse to the next tuple element.
    return pack_sequence(packer, std::forward<Tail>(tail)...);
}

static inline
void deallocate(void * data, void * hint) {
    free(data);
}

template<
    class Tag,
    class SharingPolicy
>
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

        // Sending

        template<class Event>
        typename boost::enable_if<
            boost::is_same<Tag, typename Event::tag>,
            bool
        >::type
        send_message(const message<Event>& object) {
            const bool multipart = length<
                typename event_traits<Event>::tuple_type
            >::value;

            return this->send(message<Event>::value, multipart ? ZMQ_SNDMORE : 0) &&
                   this->send_tuple(object);
        }

        template<class Event, typename... Args>
        typename boost::enable_if<
            boost::is_same<Tag, typename Event::tag>,
            bool
        >::type
        send_messagex(Args&&... args) {
            const bool multipart = length<
                typename event_traits<Event>::tuple_type
            >::value;

            bool success = this->send(message<Event>::value, multipart ? ZMQ_SNDMORE : 0);

            if(success && multipart) {
                msgpack::sbuffer buffer;
                msgpack::packer<msgpack::sbuffer> packer(buffer);

                packer.pack_array(sizeof...(Args));
                pack_sequence(packer, std::forward<Args>(args)...);

                zmq::message_t message(buffer.release(), buffer.size(), &deallocate, NULL);
                //zmq::message_t message(buffer.size());
                //memcpy(message.data(), buffer.data(), buffer.size());
                    
                return this->send(message);
            }
        }
        
        bool
        send_message(int message_id,
                     const std::string& message)
        {
            return this->send(message_id, message.size() ? ZMQ_SNDMORE : 0) &&
                   (!message.size() || this->send(protect(message)));
        }

    private:
        template<class Event>
        typename boost::enable_if<
            boost::is_same<
                typename event_traits<Event>::tuple_type,
                tuple<>
            >,
            bool
        >::type
        send_tuple(const message<Event>&) {
            return true;
        }

        template<class Event>
        typename boost::disable_if<
            boost::is_same<
                typename event_traits<Event>::tuple_type,
                tuple<>
            >,
            bool
        >::type
        send_tuple(const message<Event>& object) {
            return this->send(object);
        }
};

}} // namespace cocaine::io

#endif
