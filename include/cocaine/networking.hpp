#ifndef COCAINE_NETWORKING_HPP
#define COCAINE_NETWORKING_HPP

#include <boost/tuple/tuple.hpp>

#include <zmq.hpp>

#if ZMQ_VERSION < 20107
    #error ZeroMQ version 2.1.7+ required!
#endif

#include <msgpack.hpp>

#include "cocaine/common.hpp"
#include "cocaine/helpers/tuples.hpp"

namespace cocaine { namespace networking {

using namespace boost::tuples;

typedef std::vector<std::string> route_t;

class socket_t: 
    public boost::noncopyable,
    public birth_control_t<socket_t>
{
    public:
        socket_t(zmq::context_t& context, int type, std::string route = ""):
            m_socket(context, type),
            m_route(route)
        {
            if(!route.empty()) {
                setsockopt(ZMQ_IDENTITY, route.data(), route.size());
            } 
        }

        void bind(const std::string& endpoint) {
            m_socket.bind(endpoint.c_str());

            // Try to determine the connection string for clients
            // TODO: Do it the right way
            size_t position = endpoint.find_last_of(":");

            if(position != std::string::npos) {
                m_endpoint = config_t::get().core.hostname +
                    endpoint.substr(position, std::string::npos);
            } else {
                m_endpoint = "<local>";
            }
        }

        void connect(const std::string& endpoint) {
            m_socket.connect(endpoint.c_str());
        }
       
        bool send(zmq::message_t& message, int flags = 0) {
            try {
                return m_socket.send(message, flags);
            } catch(const zmq::error_t& e) {
                syslog(LOG_ERR, "net: [%s()] %s", __func__, e.what());
                return false;
            }
        }

        bool recv(zmq::message_t* message, int flags = 0) {
            try {
                return m_socket.recv(message, flags);
            } catch(const zmq::error_t& e) {
                syslog(LOG_ERR, "net: [%s()] %s", __func__, e.what());
                return false;
            }
        }
        
        void drop_remaining_parts() {
            zmq::message_t null;

            while(more()) {
                recv(&null);
            }
        }
        
        void getsockopt(int name, void* value, size_t* size) {
            m_socket.getsockopt(name, value, size);
        }

        void setsockopt(int name, const void* value, size_t size) {
            m_socket.setsockopt(name, value, size);
        }

    public:
        int fd() {
            int fd;
            size_t size = sizeof(fd);

            getsockopt(ZMQ_FD, &fd, &size);

            return fd;
        }

        std::string endpoint() const { 
            return m_endpoint; 
        }

        std::string route() const { 
            return m_route; 
        }

        bool pending(int event = ZMQ_POLLIN) {
            unsigned long events;
            size_t size = sizeof(events);

            getsockopt(ZMQ_EVENTS, &events, &size);

            return events & event;
        }

        bool more() {
            int64_t rcvmore;
            size_t size = sizeof(rcvmore);

            getsockopt(ZMQ_RCVMORE, &rcvmore, &size);

            return rcvmore != 0;
        }

    private:
        zmq::socket_t m_socket;
        std::string m_endpoint, m_route;
};

template<class T> class raw;

template<class T>
inline static raw<T> protect(T& object) {
    return raw<T>(object);
}

template<class T>
inline static raw<const T> protect(const T& object) {
    return raw<const T>(object);
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
        channel_t(zmq::context_t& context, int type, std::string route = ""):
            socket_t(context, type, route)
        { }

        // Bring original methods into the scope
        using socket_t::send;
        using socket_t::recv;

        // Packs and sends a single object
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
        inline bool send(const raw<T>& object, int flags) {
            zmq::message_t message;
            object.pack(message);
            return send(message, flags);
        }

        // Packs and sends a tuple
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

        // Receives and unpacks a single object
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
                    static_cast<const char*>(message.data()), message.size());
                msgpack::object object = unpacked.get();
                object.convert(&result);
            // TODO: Figure out the msgpack exception type
            } catch(const std::exception& e) {
                syslog(LOG_ERR, "net: [%s()] corrupted object - %s", __func__, e.what());
                return false;
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

        // Receives and unpacks a tuple
        inline bool recv_multi(const null_type&, int flags = 0) {
            return true;
        }

        template<class Head, class Tail>
        inline bool recv_multi(cons<Head, Tail>& o, int flags = 0) {
            return (recv(o.get_head(), flags)
                    && recv_multi(o.get_tail(), flags));
        }
};

}}

#endif
