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

#include <boost/mpl/int.hpp>
#include <boost/type_traits/remove_const.hpp>
#include <boost/tuple/tuple.hpp>
#include <msgpack.hpp>

#include <zmq.hpp>

#if ZMQ_VERSION < 20200
    #error ZeroMQ version 2.2.0+ required!
#endif

#include "cocaine/common.hpp"
#include "cocaine/rpc.hpp"

#define HOSTNAME_MAX_LENGTH 256

namespace cocaine { namespace io {

using namespace boost::tuples;

typedef std::vector<std::string> route_t;

class socket_t: 
    public boost::noncopyable
{
    public:
        socket_t(zmq::context_t& context, int type):
            m_socket(context, type)
        { }

        socket_t(zmq::context_t& context, int type, const std::string& route):
            m_socket(context, type)
        {
            setsockopt(ZMQ_IDENTITY, route.data(), route.size());
        }

        void bind(const std::string& endpoint) {
            m_socket.bind(endpoint.c_str());

            // Try to determine the connection string for clients.
            // XXX: Fix it when migrating to ZeroMQ 3.1+
            size_t position = endpoint.find_last_of(":");
            char hostname[HOSTNAME_MAX_LENGTH];

            if(gethostname(hostname, HOSTNAME_MAX_LENGTH) != 0) {
                throw system_error_t("failed to determine the hostname");
            }
            
            if(position != std::string::npos) {
                m_endpoint = std::string("tcp://")
                             + hostname
                             + endpoint.substr(position, std::string::npos);
            } else {
                m_endpoint = "<local>";
            }
        }

        void connect(const std::string& endpoint) {
            m_socket.connect(endpoint.c_str());
        }
       
        bool send(zmq::message_t& message,
                  int flags = 0)
        {
            return m_socket.send(message, flags);
        }

        bool recv(zmq::message_t * message,
                  int flags = 0)
        {
            return m_socket.recv(message, flags);
        }
                
        void getsockopt(int name,
                        void * value,
                        size_t * size) 
        {
            m_socket.getsockopt(name, value, size);
        }

        void setsockopt(int name,
                        const void * value,
                        size_t size) 
        {
            m_socket.setsockopt(name, value, size);
        }

        void drop() {
            zmq::message_t null;

            while(more()) {
                recv(&null);
            }
        }

    public:
        std::string endpoint() const { 
            return m_endpoint; 
        }

        bool more() {
            int64_t rcvmore = 0;
            size_t size = sizeof(rcvmore);

            getsockopt(ZMQ_RCVMORE, &rcvmore, &size);

            return rcvmore != 0;
        }

        std::string route() {
            char identity[256] = { 0 };
            size_t size = sizeof(identity);

            getsockopt(ZMQ_IDENTITY, &identity, &size);

            return identity;
        }

        int fd() {
            int fd = 0;
            size_t size = sizeof(fd);

            getsockopt(ZMQ_FD, &fd, &size);

            return fd;
        }

        bool pending(unsigned long event = ZMQ_POLLIN) {
            unsigned long events = 0;
            size_t size = sizeof(events);

            getsockopt(ZMQ_EVENTS, &events, &size);

            return (events & event) == event;
        }

    private:
        zmq::socket_t m_socket;
        std::string m_endpoint;
};

// RAII socket options
// -------------------

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

// A wrapper class to disable automatic type serialization.
// --------------------------------------------------------

template<class T>
struct raw {
    raw(T& value_):
        value(value_)
    { }

    T& value;
};

// Specialize this to disable specific type serialization.
template<class T>
struct serialization_traits;

template<>
struct serialization_traits<std::string> {
    static void pack(zmq::message_t& message, const std::string& value) {
        message.rebuild(value.size());
        memcpy(message.data(), value.data(), value.size());
    }

    static bool unpack(zmq::message_t& message, std::string& value) {
        value.assign(
            static_cast<const char*>(message.data()),
            message.size()
        );

        return true;
    }
};

template<class T>
static inline raw<T>
protect(T& object) {
    return raw<T>(object);
}

// Tuple-based RPC channel
// -----------------------

class channel_t:
    public socket_t
{
    public:
        channel_t(zmq::context_t& context, int type):
            socket_t(context, type)
        { }

        channel_t(zmq::context_t& context, int type, const std::string& route):
            socket_t(context, type, route)
        { }

        using socket_t::send;
        using socket_t::recv;

        // Sending
        // -------

        template<class T>
        bool send(const T& value,
                  int flags = 0)
        {
            msgpack::sbuffer buffer;
            msgpack::pack(buffer, value);
            
            zmq::message_t message(buffer.size());
            memcpy(message.data(), buffer.data(), buffer.size());
            
            return send(message, flags);
        }
        
        template<class T>
        bool send(const raw<T>& object,
                  int flags = 0)
        {
            zmq::message_t message;
            
            serialization_traits<
                typename boost::remove_const<T>::type
            >::pack(message, object.value);
            
            return send(message, flags);
        }

        template<int Code>
        bool send(rpc::packed<Code>& command,
                  int flags = 0)
        {
            const bool multipart = boost::tuples::length<
                typename rpc::packed<Code>::tuple_type
            >::value;

            if(multipart) {
                return send(Code, ZMQ_SNDMORE | flags) &&
                       send_multipart(command, flags);
            } else {
                return send(Code, flags);
            }
        }

        // Receiving
        // ---------

        template<class T>
        bool recv(T& result,
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
                
                unpacked.get().convert(&result);
            } catch(const msgpack::type_error& e) {
                throw std::runtime_error("corrupted object");
            } catch(const std::bad_cast& e) {
                throw std::runtime_error("corrupted object - type mismatch");
            }

            return true;
        }
      
        template<class T>
        bool recv(raw<T>& result,
                  int flags = 0)
        {
            zmq::message_t message;

            if(!recv(&message, flags)) {
                return false;
            }

            return serialization_traits<
                typename boost::remove_const<T>::type
            >::unpack(message, result.value);
        }

        bool recv_multi(const null_type&,
                        int __attribute__ ((unused)) flags = 0)
        {
            return true;
        }

        template<class Head>
        bool recv_multi(cons<Head, null_type>& o,
                        int flags = 0)
        {
            return recv(o.get_head(), flags);
        }

        template<class Head, class Tail>
        bool recv_multi(cons<Head, Tail>& o,
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
        bool send_multipart(const null_type&,
                            int __attribute__ ((unused)) flags = 0)
        {
            return true;
        }

        template<class Head>
        bool send_multipart(const cons<Head, null_type>& o,
                            int flags = 0)
        {
            return send(o.get_head(), flags);
        }

        template<class Head, class Tail>
        bool send_multipart(const cons<Head, Tail>& o,
                            int flags = 0)
        {
            return send(o.get_head(), ZMQ_SNDMORE | flags) &&
                   send_multipart(o.get_tail(), flags);
        }
};

}}

#endif
