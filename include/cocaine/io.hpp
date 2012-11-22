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
#include <boost/mpl/size.hpp>

#include <boost/thread/mutex.hpp>
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

namespace detail {
    template<class T>
    struct raw {
        raw(T& value_):
            value(value_)
        { }

        T& value;
    };
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

template<class T>
static inline
detail::raw<T>
protect(T& object) {
    return detail::raw<T>(object);
}

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

    protected:
        zmq::socket_t m_socket;
        
    private:
        context_t& m_context;

        int m_fd;
        std::string m_endpoint;

        uint16_t m_port;
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
        scoped_option(socket_base_t& socket,
                      value_type value):
            m_socket(socket),
            m_saved(value_type()),
            m_size(sizeof(m_saved))
        {
            m_socket.getsockopt(option_type(), &m_saved, &m_size);
            m_socket.setsockopt(option_type(), &value, sizeof(value));
        }

        ~scoped_option() {
            m_socket.setsockopt(option_type(), &m_saved, m_size);
        }

    private:
        socket_base_t& m_socket;
        
        value_type m_saved;
        size_t m_size;
};

// Event tuple type extraction

template<class Tag>
struct dispatch;

namespace detail {
    template<class T>
    struct depend {
        typedef void type;
    };

    template<class Event, class Tuple = void>
    struct event_traits {
        typedef boost::mpl::list<> tuple_type;
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
}

// RPC message

template<class Event>
struct message {
    enum constants {
        id = detail::enumerate<Event>::value,
        length = boost::mpl::size<typename detail::event_traits<Event>::tuple_type>::type::value,
        empty = length == 0
    };
};

namespace detail {
    template<class Event>
    struct outgoing:
        public message<Event>
    {
        template<typename... Args>
        outgoing(Args&&... args) {
            io::pack_sequence<
                typename event_traits<Event>::tuple_type
            >(m_buffer, std::forward<Args>(args)...);
        }

        const char*
        data() const {
            return m_buffer.data();
        }

        size_t
        size() const {
            return m_buffer.size();
        }

    private:
        msgpack::sbuffer m_buffer;
    };

    template<class Event>
    struct incoming:
        public message<Event>
    {
        template<typename... Args>
        incoming(const msgpack::object& object,
                 Args&&... args)
        {
            io::unpack_sequence<
                typename event_traits<Event>::tuple_type
            >(object, std::forward<Args>(args)...);
        }
    };
}

template<class SharingPolicy, class Tag = void>
class socket:
    public socket_base_t
{
    public:
        socket(context_t& context,
               int type):
            socket_base_t(context, type)
        { }

        // ZeroMQ messages

        bool
        send(zmq::message_t& message,
             int flags = 0)
        {
            COCAINE_EINTR_GUARD(
                return m_socket.send(message, flags)
            );
        }

        // Serialized objects

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
        
        // Custom-serialized objects

        template<class T>
        bool
        send(const detail::raw<T>& object,
             int flags = 0)
        {
            zmq::message_t message;

            raw_traits<
                typename boost::remove_const<T>::type
            >::pack(message, object.value);

            return send(message, flags);
        }      
        
        // Multipart messages

        template<class Head>
        bool
        send_multipart(Head&& head) {
            return send(head);
        }

        template<class Head, class... Tail>
        bool
        send_multipart(Head&& head,
                       Tail&&... tail)
        {
            return send(head, ZMQ_SNDMORE) &&
                   send_multipart(std::forward<Tail>(tail)...);
        }

        // RPC messages

        template<class Event, typename... Args>
        typename boost::disable_if_c<
            message<Event>::empty,
            bool
        >::type
        send(Args&&... args) {
            static_assert(
                boost::is_same<Tag, typename Event::tag>::value,
                "invalid event category"
            );

            detail::outgoing<Event> event(std::forward<Args>(args)...);

            zmq::message_t body(event.size());

            memcpy(
                body.data(),
                event.data(),
                event.size()
            );

            return send_multipart(
                static_cast<int>(message<Event>::id),
                body
            );
        }

        template<class Event>
        typename boost::enable_if_c<
            message<Event>::empty,
            bool
        >::type
        send() {
            static_assert(
                boost::is_same<Tag, typename Event::tag>::value,
                "invalid event category"
            );

            return send(
                static_cast<int>(message<Event>::id)
            );
        }
        
        // ZeroMQ messages

        bool
        recv(zmq::message_t& message,
             int flags = 0)
        {
            COCAINE_EINTR_GUARD(
                return m_socket.recv(&message, flags)
            );
        }

        // Serialized messages

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

        // Custom-serialized messages

        template<class T>
        bool
        recv(detail::raw<T>& result,
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
        recv(detail::raw<T>&& result,
             int flags = 0)
        {
            return recv(result, flags);
        }

        // Multipart messages

        template<class Head>
        bool
        recv_multipart(Head&& head) {
            return recv(head);
        }

        template<class Head, class... Tail>
        bool
        recv_multipart(Head&& head,
                       Tail&&... tail)
        {
            return recv(head) &&
                   recv_multipart(std::forward<Tail>(tail)...);
        }

        // RPC messages

        template<class Event, typename... Args>
        typename boost::disable_if_c<
            message<Event>::empty,
            bool
        >::type
        recv(Args&&... args) {
            static_assert(
                boost::is_same<Tag, typename Event::tag>::value,
                "invalid event category"
            );

            zmq::message_t message;
            msgpack::unpacked unpacked;

            if(!recv(message)) {
                return false;
            }

            try {
                msgpack::unpack(
                    &unpacked,
                    static_cast<const char*>(message.data()),
                    message.size()
                );

                detail::incoming<Event>(
                    unpacked.get(),
                    std::forward<Args>(args)...
                );
            } catch(const msgpack::type_error& e) {
                throw error_t("corrupted object");
            } catch(const std::bad_cast& e) {
                throw error_t("corrupted object - type mismatch");
            }

            return true;
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

// RPC channel

template<
    class Tag,
    class SharingPolicy
>
class channel:
    public socket<SharingPolicy, Tag>
{
    public:
        channel(context_t& context, int type):
            socket<SharingPolicy, Tag>(context, type)
        { }
        
        template<class T>
        channel(context_t& context, int type, const T& identity):
            socket<SharingPolicy, Tag>(context, type)
        {
            msgpack::sbuffer buffer;
            msgpack::packer<msgpack::sbuffer> packer(buffer);

            // NOTE: Channels allow any serializable type to be its
            // identity, for example UUIDs.
            type_traits<T>::pack(packer, identity);
            
            this->setsockopt(ZMQ_IDENTITY, buffer.data(), buffer.size());
        }

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
