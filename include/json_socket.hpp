#ifndef YAPPI_SOCKET_HPP
#define YAPPI_SOCKET_HPP

#include <map>

#include <zmq.hpp>
#include <msgpack.hpp>

#include "json/json.h"

namespace yappi { namespace core {

class json_socket_t {
    public:
        json_socket_t(zmq::context_t& context, int type):
            m_socket(context, type)
        {}

        bool pending(int event = ZMQ_POLLIN) {
            unsigned long events;
            size_t size = sizeof(events);

            m_socket.getsockopt(ZMQ_EVENTS, &events, &size);
            return events & event;
        }

        bool send(const Json::Value& root) {
            Json::FastWriter writer;
        
            std::string response = writer.write(root);
            zmq::message_t message(response.length());
            memcpy(message.data(), response.data(), response.length());

            return m_socket.send(message);
        }

        bool recv(Json::Value& root, int flags = 0) {
            zmq::message_t message;
            std::string request;
            Json::Reader reader(Json::Features::strictMode());
        
            if(!m_socket.recv(&message, flags)) {
                return false;
            }

            request.assign(
                static_cast<const char*>(message.data()),
                message.size());

            if(!reader.parse(request, root)) {
                root["error"] = reader.getFormatedErrorMessages();
                return false;
            }

            return true;
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

    private:
        zmq::socket_t m_socket;
};

}}

#endif
