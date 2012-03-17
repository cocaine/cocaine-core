//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef COCAINE_NETWORKING_HPP
#define COCAINE_NETWORKING_HPP

#include <boost/tuple/tuple.hpp>
#include <msgpack.hpp>

#include <zmq.hpp>

#if ZMQ_VERSION < 20107
    #error ZeroMQ version 2.1.7+ required!
#endif

#include "cocaine/common.hpp"

#define HOSTNAME_MAX_LENGTH 256

namespace cocaine { namespace networking {

using namespace boost::tuples;

typedef std::vector<std::string> route_t;

class socket_t: 
    public boost::noncopyable
{
    public:
        socket_t(zmq::context_t& ctx, int type):
            m_socket(ctx, type)
        { }

        socket_t(zmq::context_t& ctx, int type, const std::string& route):
            m_socket(ctx, type)
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
                m_endpoint = hostname + endpoint.substr(position, std::string::npos);
            } else {
                m_endpoint = "<local>";
            }
        }

        void connect(const std::string& endpoint) {
            m_socket.connect(endpoint.c_str());
        }
       
        bool send(zmq::message_t& message, int flags = 0) {
            return m_socket.send(message, flags);
        }

        bool recv(zmq::message_t * message, int flags = 0) {
            return m_socket.recv(message, flags);
        }
                
        void getsockopt(int name, void * value, size_t * size) {
            m_socket.getsockopt(name, value, size);
        }

        void setsockopt(int name, const void * value, size_t size) {
            m_socket.setsockopt(name, value, size);
        }

        void drop_remaining_parts() {
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

template<class T> class raw;

template<class T>
inline static raw<T> protect(T& object) {
    return raw<T>(object);
}

template<> class raw<std::string> {
    public:
        raw(std::string& string):
            m_string(string)
        { }

        inline void pack(zmq::message_t& message) const {
            message.rebuild(m_string.size());
            memcpy(message.data(), m_string.data(), m_string.size());
        }

        inline bool unpack(/* const */ zmq::message_t& message) {
            m_string.assign(
                static_cast<const char*>(message.data()),
                message.size());
            return true;
        }

    private:
        std::string& m_string;
};

template<> class raw<const std::string> {
    public:
        raw(const std::string& string):
            m_string(string)
        { }

        inline void pack(zmq::message_t& message) const {
            message.rebuild(m_string.size());
            memcpy(message.data(), m_string.data(), m_string.size());
        }

    private:
        const std::string& m_string;
};

class channel_t:
    public socket_t
{
    public:
        channel_t(zmq::context_t& ctx, int type):
            socket_t(ctx, type)
        { }

        channel_t(zmq::context_t& ctx, int type, const std::string& route):
            socket_t(ctx, type, route)
        { }

        // Brings the original methods into the scope.
        using socket_t::send;
        using socket_t::recv;

        // Packs and sends a single object.
        template<class T>
        bool send(const T& value, int flags = 0) {
            zmq::message_t message;
            
            msgpack::sbuffer buffer;
            msgpack::pack(buffer, value);
            
            message.rebuild(buffer.size());
            memcpy(message.data(), buffer.data(), buffer.size());
            
            return send(message, flags);
        }
        
        template<class T>
        inline bool send(const raw<T>& object, int flags = 0) {
            zmq::message_t message;
            object.pack(message);
            return send(message, flags);
        }

        // Packs and sends a tuple.
        inline bool send_multi(const null_type&, int flags = 0) {
            return true;
        }

        template<class Head>
        inline bool send_multi(const cons<Head, null_type>& o, int flags = 0) {
            return send(o.get_head(), flags);
        }

        template<class Head, class Tail>
        inline bool send_multi(const cons<Head, Tail>& o, int flags = 0) {
            return (send(o.get_head(), ZMQ_SNDMORE | flags) 
                    && send_multi(o.get_tail(), flags));
        }

        // Receives and unpacks a single object.
        template<class T>
        bool recv(T& result, int flags = 0) {
            zmq::message_t message;
            msgpack::unpacked unpacked;

            if(!recv(&message, flags)) {
                return false;
            }
           
            try { 
                msgpack::unpack(
                    &unpacked,
                    static_cast<const char*>(message.data()), message.size()
                );
                
                unpacked.get().convert(&result);
            } catch(const msgpack::unpack_error& e) {
                throw std::runtime_error("corrupted object");
            } catch(const std::bad_cast& e) {
                throw std::runtime_error("corrupted object - type mismatch");
            }

            return true;
        }
      
        template<class T>
        inline bool recv(raw<T>& result, int flags) {
            zmq::message_t message;

            if(!recv(&message, flags)) {
                return false;
            }

            return result.unpack(message);
        }

        // Receives and unpacks a tuple.
        inline bool recv_multi(const null_type&, int flags = 0) {
            return true;
        }

        template<class Head, class Tail>
        inline bool recv_multi(cons<Head, Tail>& o, int flags = 0) {
            return (recv(o.get_head(), flags)
                    && recv_multi(o.get_tail(), flags));
        }
};

struct route {
    route(channel_t& channel_):
        channel(channel_)
    { }

    template<class T>
    void operator()(const T& route) {
        channel.send(protect(route), ZMQ_SNDMORE);
    }

    channel_t& channel;
};

}}

#endif
