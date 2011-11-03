#ifndef COCAINE_DEFERRED_HPP
#define COCAINE_DEFERRED_HPP

#include <boost/enable_shared_from_this.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/lines.hpp"

namespace cocaine { namespace lines {

class deferred_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<deferred_t>,
    public birth_control_t<deferred_t>
{
    public:
        deferred_t(const std::string& method);
        virtual ~deferred_t() { }

    public:
        void enqueue(engine::engine_t* engine);

    public:
        virtual void send(zmq::message_t& chunk) = 0;
        virtual void abort(const std::string& error) = 0;
        
    public:
        zmq::message_t& request();

    private:
        std::string m_method;
        zmq::message_t m_request;
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
            deferred_t(key),
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
                abort("the result must be a json object");
            }
        }

        virtual void abort(const std::string& error) {
            Json::Value object(Json::objectValue);
            
            object["error"] = error;
            
            m_publisher->publish(m_key, object);
        }

    private:
        const std::string m_key;
        publisher_t* m_publisher;
};

}}

#endif
