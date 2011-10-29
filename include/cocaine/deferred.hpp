#ifndef COCAINE_DEFERRED_HPP
#define COCAINE_DEFERRED_HPP

#include "cocaine/common.hpp"
#include "cocaine/networking.hpp"

namespace cocaine { namespace lines {

class deferred_t:
    public boost::noncopyable,
    public unique_id_t,
    public birth_control_t<deferred_t>
{
    public:
        virtual void send(zmq::message_t& chunk) = 0;
        virtual void abort(const std::string& error) = 0;
};

class publisher_t {
    public:
        virtual void publish(const std::string& key, const Json::Value& object) = 0;
};

class publication_t:
    public deferred_t
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
                abort("the result must be a json object");
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
