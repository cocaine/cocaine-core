#ifndef COCAINE_NETWORKING_HPP
#define COCAINE_NETWORKING_HPP

#include <zmq.hpp>

#if ZMQ_VERSION < 20107
    #error ZeroMQ version 2.1.7+ required!
#endif

#include <msgpack.hpp>

#include "cocaine/common.hpp"

namespace cocaine { namespace net {

enum CommandCode {
    PUSH = 1,   /* engine pushes a task to an overseer */
    DROP,       /* engine drops a task from an overseer */
    TERMINATE,  /* engine terminates an overseer */
    FULFILL,    /* overseer fulfills an engine's request */
    SUICIDE,    /* overseer performs a suicide */
    WATCH,      /* overseer is entering the plugin code, asking for a watchdog */
    UNWATCH     /* overseer is finished with the plugin code, stops the watchdog */
};

class blob_socket_t: 
    public boost::noncopyable,
    public helpers::birth_control_t<blob_socket_t>
{
    public:
        blob_socket_t(zmq::context_t& context, int type):
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

class msgpack_socket_t:
    public blob_socket_t
{
    public:
        msgpack_socket_t(zmq::context_t& context, int type):
            blob_socket_t(context, type)
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
                syslog(LOG_ERR, "net: invalid data format - %s", e.what());
                return false;
            }

            return true;
        }
};

class json_socket_t:
    public msgpack_socket_t
{
    public:
        json_socket_t(zmq::context_t& context, int type):
            msgpack_socket_t(context, type)
        {}

        bool send_json(const Json::Value& root, int flags = 0);
        bool recv_json(Json::Value& root, int flags = 0);
};

}}

#endif
