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

#include "cocaine/common.hpp"
#include "cocaine/traits.hpp"

#include "cocaine/helpers/birth_control.hpp"

#include <type_traits>

#include <boost/mpl/begin.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/distance.hpp>
#include <boost/mpl/find.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/size.hpp>

#include <boost/thread/mutex.hpp>

#include <zmq.hpp>

#if ZMQ_VERSION < 20200
    #error ZeroMQ version 2.2.0+ required!
#endif

namespace cocaine { namespace io {

// ZeroMQ socket

class socket_base_t: 
    public boost::noncopyable,
    public birth_control<socket_base_t>
{
    public:
        socket_base_t(context_t& context,
                      int type);

        virtual
        ~socket_base_t();

        void
        bind();

        void
        bind(const std::string& endpoint);
        
        void
        connect(const std::string& endpoint);

        bool
        send(zmq::message_t& message,
             int flags = 0);

        bool
        recv(zmq::message_t& message,
             int flags = 0);

        void
        drop();

        void
        getsockopt(int name,
                   void * value,
                   size_t * size);

        void
        setsockopt(int name,
                   const void * value,
                   size_t size);

    public:
        int
        fd() const {
            return m_fd;
        }

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

        bool
        pending(unsigned long event = ZMQ_POLLIN) {
            unsigned long events = 0;
            size_t size = sizeof(events);

            getsockopt(ZMQ_EVENTS, &events, &size);

            return events & event;
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
        
        // NOTE: Keep the previous values so that the socket state could be
        // restored on scope exit.
        value_type m_saved;
        size_t m_size;
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

// Custom serialization

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
        
        std::memcpy(
            message.data(),
            value.data(),
            value.size()
        );
    }

    static
    void
    unpack(/* const */ zmq::message_t& message,
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

// Shareable serializing socket

template<class SharingPolicy>
class socket:
    public socket_base_t
{
    public:
        socket(context_t& context,
               int type):
            socket_base_t(context, type)
        { }

        using socket_base_t::send;
        using socket_base_t::recv;

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
            
            std::memcpy(
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
                typename std::remove_const<T>::type
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
               
                type_traits<T>::unpack(
                    unpacked.get(),
                    result
                );
            } catch(const msgpack::type_error& e) {
                throw cocaine::error_t("corrupted object");
            } catch(const std::bad_cast& e) {
                throw cocaine::error_t("corrupted object - type mismatch");
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
                typename std::remove_const<T>::type
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

        // Lockable concept implementation

        void lock() {
            m_mutex.lock();
        }

        void unlock() {
            m_mutex.unlock();
        }

    private:
        typename SharingPolicy::mutex_type m_mutex;
};

// RPC channel

namespace mpl = boost::mpl;

template<class Tag>
struct dispatch;

namespace detail {
    template<
        class Event,
        class Category = typename dispatch<
            typename Event::tag
        >::category
    >
    struct enumerate:
        public mpl::distance<
            typename mpl::begin<Category>::type,
            typename mpl::find<Category, Event>::type
        >::type
    {
        static_assert(
            mpl::contains<Category, Event>::value,
            "event has not been enumerated within its category"
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
    typedef typename detail::tuple_type<Event>::type tuple_type;

    enum constants {
        id = detail::enumerate<Event>::value,
        length = mpl::size<tuple_type>::value,
        empty = length == 0
    };
};

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

        using socket<SharingPolicy>::send;
        using socket<SharingPolicy>::recv;

        // RPC messages

        template<class Event, typename... Args>
        typename boost::disable_if_c<
            event_traits<Event>::empty,
            bool
        >::type
        send(Args&&... args) {
            static_assert(
                boost::is_same<Tag, typename Event::tag>::value,
                "invalid event category"
            );

            msgpack::sbuffer buffer;

            io::pack_sequence<
                typename event_traits<Event>::tuple_type
            >(buffer, std::forward<Args>(args)...);

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
        typename boost::enable_if_c<
            event_traits<Event>::empty,
            bool
        >::type
        send() {
            static_assert(
                boost::is_same<Tag, typename Event::tag>::value,
                "invalid event category"
            );

            return this->send(
                static_cast<int>(event_traits<Event>::id)
            );
        }

        template<class Event, typename... Args>
        typename boost::disable_if_c<
            event_traits<Event>::empty,
            bool
        >::type
        recv(Args&&... args) {
            static_assert(
                boost::is_same<Tag, typename Event::tag>::value,
                "invalid event category"
            );

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

                io::unpack_sequence<
                    typename event_traits<Event>::tuple_type
                >(unpacked.get(), std::forward<Args>(args)...);
            } catch(const msgpack::type_error& e) {
                throw cocaine::error_t("corrupted object");
            } catch(const std::bad_cast& e) {
                throw cocaine::error_t("corrupted object - type mismatch");
            }

            return true;
        }

        // XXX: This have to go.

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
