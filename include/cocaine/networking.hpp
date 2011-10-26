#ifndef COCAINE_NETWORKING_HPP
#define COCAINE_NETWORKING_HPP

#include <zmq.hpp>

#if ZMQ_VERSION < 20107
    #error ZeroMQ version 2.1.7+ required!
#endif

#include <msgpack.hpp>

#include "cocaine/common.hpp"
#include "cocaine/networking/raw.hpp"
#include "cocaine/networking/tuples.hpp"

// Message types
#define INVOKE      1    /* engine -> worker: do something*/
#define TERMINATE   2    /* engine -> worker: die */

#define CHUNK       10   /* worker -> engine: something is in progress, here's the part of the result */
#define CHOKE       11   /* worker -> engine: something is done, choke the channel */
#define SUICIDE     12   /* worker -> engine: i am useless, kill me */
#define HEARTBEAT   13   /* worker -> engine: i am alive, don't kill me */

namespace cocaine { namespace lines {

using namespace boost::tuples;

typedef std::vector<std::string> route_t;

class socket_t: 
    public boost::noncopyable,
    public birth_control_t<socket_t>
{
    public:
        socket_t(zmq::context_t& context, int type, std::string identity = ""):
            m_socket(context, type)
        {
            if(!identity.empty()) {
                setsockopt(ZMQ_IDENTITY, identity.data(), identity.length());
            } 
        }

        inline bool send(zmq::message_t& message, int flags = 0) {
            try {
                return m_socket.send(message, flags);
            } catch(const zmq::error_t& e) {
                syslog(LOG_ERR, "net: [%s()] %s", __func__, e.what());
                return false;
            }
        }

        inline bool recv(zmq::message_t* message, int flags = 0) {
            try {
                return m_socket.recv(message, flags);
            } catch(const zmq::error_t& e) {
                syslog(LOG_ERR, "net: [%s()] %s", __func__, e.what());
                return false;
            }
        }

        inline void getsockopt(int name, void* value, size_t* length) {
            m_socket.getsockopt(name, value, length);
        }

        inline void setsockopt(int name, const void* value, size_t length) {
            m_socket.setsockopt(name, value, length);
        }

        inline void bind(const std::string& endpoint) {
            m_socket.bind(endpoint.c_str());
        }

        inline void connect(const std::string& endpoint) {
            m_socket.connect(endpoint.c_str());
        }
       
    public:
        int fd();
        std::string identity();

        bool pending(int event = ZMQ_POLLIN);
        bool more();
        
#if ZMQ_VERSION > 30000
        bool label();
#endif

    private:
        zmq::socket_t m_socket;
};

class channel_t:
    public socket_t
{
    public:
        channel_t(zmq::context_t& context, int type, std::string identity = ""):
            socket_t(context, type, identity)
        {}

        // Bring original methods into the scope
        using socket_t::send;
        using socket_t::recv;

        // Drops the current message
        void ignore() {
            zmq::message_t null;

            while(more()) {
                recv(&null);
            }
        }

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
        bool send(const raw<T>& object, int flags) {
            zmq::message_t message;
            object.pack(message);
            return send(message, flags);
        }

        // Packs and sends a tuple
        bool send_multi(const null_type&, int flags = 0) {
            return true;
        }

        template<class Head>
        bool send_multi(const cons<Head, null_type>& o, int flags = 0) {
            return send(o.get_head(), flags);
        }

        template<class Head, class Tail>
        bool send_multi(const cons<Head, Tail>& o, int flags = 0) {
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
                msgpack::unpack(&unpacked,
                    static_cast<const char*>(message.data()),
                    message.size());
                msgpack::object object = unpacked.get();
                object.convert(&result);
            } catch(const std::exception& e) {
                syslog(LOG_ERR, "net: [%s()] corrupted object - %s", __func__, e.what());
                return false;
            }

            return true;
        }
      
        template<class T>
        bool recv(raw<T>& result, int flags) {
            zmq::message_t message;

            if(!recv(&message, flags)) {
                return false;
            }

            return result.unpack(message);
        }

        // Receives and unpacks a tuple
        bool recv_multi(const null_type&, int flags = 0) {
            return true;
        }

        template<class Head, class Tail>
        bool recv_multi(cons<Head, Tail>& o, int flags = 0) {
            return (recv(o.get_head(), flags)
                    && recv_multi(o.get_tail(), flags));
        }
};

}}

namespace msgpack {
    template<class Stream>
    packer<Stream>& operator<<(packer<Stream>& o, const Json::Value& v) {
        Json::FastWriter writer;
        std::string json(writer.write(v));

        o.pack_raw(json.size());
        o.pack_raw_body(json.data(), json.size());

        return o;
    }

    template<>
    Json::Value& operator>>(msgpack::object o, Json::Value& v);
}

#endif
