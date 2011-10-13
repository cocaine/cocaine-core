#ifndef COCAINE_RESPONSE_HPP
#define COCAINE_RESPONSE_HPP

#include "cocaine/common.hpp"
#include "cocaine/networking.hpp"

namespace cocaine { namespace core {

class response_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<response_t>,
    public helpers::birth_control_t<response_t>
{
    public:
        response_t(const lines::route_t& route, boost::shared_ptr<lines::socket_t> socket):
            m_route(route),
            m_socket(socket)
        {
            m_root["hostname"] = config_t::get().core.hostname;
        }

        ~response_t() {
            if(m_socket) {
                zmq::message_t message;
                
                // Send the identity
                for(lines::route_t::const_iterator id = m_route.begin(); id != m_route.end(); ++id) {
                    message.rebuild(id->length());
                    memcpy(message.data(), id->data(), id->length());
                    m_socket->send(message, ZMQ_SNDMORE);
                }
                
                // Send the delimiter
                message.rebuild(0);
                m_socket->send(message, ZMQ_SNDMORE);

                Json::FastWriter writer;
                std::string json(writer.write(m_root));
                message.rebuild(json.length());
                memcpy(message.data(), json.data(), json.length());

                m_socket->send(message);
            }
        }

    public:
        template<class T>
        void wait(boost::shared_ptr<T> object) {
            object->bind("", shared_from_this());
        }

        template<class T>
        void wait(const std::string& key, boost::shared_ptr<T> object) {
            object->bind(key, shared_from_this());
        }

        void push(const std::string& key, const Json::Value& object) {
            if(key.empty()) {
                m_root = object;
            } else {
                m_root[key] = object;
            }
        }

        void abort(const std::string& error) {
            m_root.clear();
            m_root["error"] = error;
        }

        void abort(const std::string& key, const std::string& error) {
            Json::Value object;
            object["error"] = error;
            
            push(key, object);
        }
        
    private:
        lines::route_t m_route;
        boost::shared_ptr<lines::socket_t> m_socket;
        
        Json::Value m_root;
};

}}

#endif
