#ifndef COCAINE_DEFERRED_HPP
#define COCAINE_DEFERRED_HPP

#include "cocaine/common.hpp"
#include "cocaine/networking.hpp"

namespace cocaine { namespace lines {

class deferred_t:
    public boost::noncopyable,
    public unique_id_t
{
    public:
        virtual void send(zmq::message_t& chunk) = 0;
        virtual void abort(const std::string& error) = 0;
};

class responder_t {
    public:
        virtual void respond(const route_t& route, zmq::message_t& chunk) = 0;
};

class publisher_t {
    public:
        virtual void publish(const std::string& key, const Json::Value& object) = 0;
};


class response_t:
    public deferred_t,
    public birth_control_t<response_t>
{
    public:
        response_t(const lines::route_t& route, boost::shared_ptr<responder_t> parent):
            m_route(route),
            m_parent(parent)
        { }

        virtual void send(zmq::message_t& chunk) {
            m_parent->respond(m_route, chunk);
        }

        virtual void abort(const std::string& error) {
            Json::Value object(Json::objectValue);

            object["error"] = error;
            
            Json::FastWriter writer;
            std::string response(writer.write(object));
            zmq::message_t message;
            
            memcpy(message.data(), response.data(), response.size());
            
            m_parent->respond(m_route, message);
        }

    private:
        const lines::route_t m_route;
        const boost::shared_ptr<responder_t> m_parent;
};

class publication_t:
    public deferred_t,
    public birth_control_t<publication_t>
{
    public:
        publication_t(const std::string& key, boost::shared_ptr<publisher_t> parent):
            m_key(key),
            m_parent(parent)
        { }

        virtual void send(zmq::message_t& chunk) {
            Json::Reader reader(Json::Features::strictMode());
            Json::Value root;

            if(reader.parse(
                static_cast<const char*>(chunk.data()),
                static_cast<const char*>(chunk.data()) + chunk.size(),
                root))
            {
                m_parent->publish(m_key, root);
            } else {
                abort("invalid publication format");
            }
        }

        virtual void abort(const std::string& error) {
            Json::Value object(Json::objectValue);

            object["error"] = error;
            
            m_parent->publish(m_key, object);
        }

    private:
        const std::string m_key;
        const boost::shared_ptr<publisher_t> m_parent;
};

}}

#endif
