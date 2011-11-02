#ifndef COCAINE_DEFERRED_HPP
#define COCAINE_DEFERRED_HPP

#include "cocaine/common.hpp"
#include "cocaine/networking.hpp"

namespace cocaine { namespace lines {

class deferred_t:
    public boost::noncopyable,
    public birth_control_t<deferred_t>
{
    public:
        virtual void send(zmq::message_t& chunk) = 0;

        void send_json(const Json::Value& object) {
            Json::FastWriter writer;
            std::string response(writer.write(object));

            zmq::message_t message(response.size());
            memcpy(message.data(), response.data(), response.size());

            send(message);
        }
};

class publisher_t {
    public:
        virtual void publish(const std::string& key, const Json::Value& object) = 0;
};

class publication_t:
    public deferred_t
{
    public:
        publication_t(const std::string& key, publisher_t* publisher):
            m_key(key),
            m_publisher(publisher)
        { }

        virtual void send(zmq::message_t& chunk) {
            Json::Reader reader(Json::Features::strictMode());
            Json::Value root;

            if(reader.parse(
                static_cast<const char*>(chunk.data()),
                static_cast<const char*>(chunk.data()) + chunk.size(),
                root))
            {
                m_publisher->publish(m_key, root);
            } else {
                m_publisher->publish(m_key, helpers::make_json(
                    "error", "the result must be a json object"));
            }
        }

    private:
        const std::string m_key;
        publisher_t* m_publisher;
};

}}

#endif
