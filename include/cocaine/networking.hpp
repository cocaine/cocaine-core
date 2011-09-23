#ifndef COCAINE_NETWORKING_HPP
#define COCAINE_NETWORKING_HPP

#include <zmq.hpp>

#if ZMQ_VERSION < 20107
    #error ZeroMQ version 2.1.7+ required!
#endif

#include <msgpack.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace lines {

using namespace boost::tuples;

class socket_t: 
    public boost::noncopyable,
    public helpers::birth_control_t<socket_t>
{
    public:
        socket_t(zmq::context_t& context, int type):
            m_socket(context, type)
        {}

        inline bool send(zmq::message_t& message, int flags = 0) {
            try {
                return m_socket.send(message, flags);
            } catch(const zmq::error_t& e) {
                syslog(LOG_ERR, "net: send() failed - %s", e.what());
                return false;
            }
        }

        inline bool recv(zmq::message_t* message, int flags = 0) {
            try {
                return m_socket.recv(message, flags);
            } catch(const zmq::error_t& e) {
                syslog(LOG_ERR, "net: recv() failed - %s", e.what());
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
        bool pending(int event = ZMQ_POLLIN);
        bool has_more();

    private:
        zmq::socket_t m_socket;
};

#define PUSH      1 /* engine pushes a task to an overseer */
#define DROP      2 /* engine drops a task from an overseer */
#define TERMINATE 3 /* engine terminates an overseer */
#define FUTURE    4 /* overseer fulfills an engine's request */
#define SUICIDE   5 /* overseer performs a suicide */

class channel_t:
    public socket_t
{
    public:
        channel_t(zmq::context_t& context, int type):
            socket_t(context, type)
        {}

        template<class T>
        bool send_object(const T& value, int flags = 0) {
            zmq::message_t message;
            
            msgpack::sbuffer buffer;
            msgpack::pack(buffer, value);
            
            message.rebuild(buffer.size());
            memcpy(message.data(), buffer.data(), buffer.size());
            
            return send(message, flags);
        }

        bool send_tuple(const null_type&, int flags = 0) {
            return true;
        }

        template<class Head>
        bool send_tuple(const cons<Head, null_type>& o, int flags = 0) {
            return send_object(o.get_head(), flags);
        }

        template<class Head, class Tail>
        bool send_tuple(const cons<Head, Tail>& o, int flags = 0) {
            return (send_object(o.get_head(), ZMQ_SNDMORE | flags) 
                    && send_tuple(o.get_tail(), flags));
        }

        template<class T>
        bool recv_object(T& result, int flags = 0) {
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
                syslog(LOG_ERR, "net: corrupted object - %s", e.what());
                return false;
            }

            return true;
        }
       
        bool recv_tuple(const null_type&, int flags = 0) {
            return true;
        }

        template<class Head, class Tail>
        bool recv_tuple(cons<Head, Tail>& o, int flags = 0) {
            return (recv_object(o.get_head(), flags)
                    && recv_tuple(o.get_tail(), flags));
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
