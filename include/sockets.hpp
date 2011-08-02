#ifndef YAPPI_SOCKETS_HPP
#define YAPPI_SOCKETS_HPP

#include <zmq.hpp>

#include "common.hpp"
#include "json/json.h"

namespace yappi { namespace net {

class blob_socket_t: public boost::noncopyable {
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
        
        bool pending(int event = ZMQ_POLLIN);
        bool has_more();
        int fd();

    private:
        zmq::socket_t m_socket;
};

class json_socket_t: public blob_socket_t {
    public:
        json_socket_t(zmq::context_t& context, int type):
            blob_socket_t(context, type)
        {}

        bool send(const Json::Value& root);
        bool recv(Json::Value& root, int flags = 0);
};

}}

#endif
