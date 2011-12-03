#ifndef COCAINE_DRIVER_NATIVE_SERVER_HPP
#define COCAINE_DRIVER_NATIVE_SERVER_HPP

#include "cocaine/drivers/zeromq_server.hpp"

namespace cocaine { namespace engine { namespace driver {

namespace messages {
    struct request_t {
        unique_id_t::type id;
        job::policy_t policy;
        msgpack::type::raw_ref body;

        MSGPACK_DEFINE(id, policy, body);
    };

    struct chunk_t {
        static const unsigned int message_code;
        
        chunk_t(const unique_id_t::type& id_, /* const */ zmq::message_t& message):
            id(id_),
            body(static_cast<const char*>(message.data()), message.size())
        { }

        unique_id_t::type id;
        msgpack::type::raw_ref body;
        
        MSGPACK_DEFINE(id, body);
    };

    struct error_t {
        static const unsigned int message_code;
        
        error_t(const unique_id_t::type& id_, unsigned int code_, const std::string& message_):
            id(id_),
            error_code(code_),
            error_message(message_)
        { }

        unique_id_t::type id;
        unsigned int error_code;
        std::string error_message;

        MSGPACK_DEFINE(id, error_code, error_message);
    };
    
    struct tag_t {
        static const unsigned int message_code;

        tag_t(const unique_id_t::type& id_):
            id(id_)
        { }

        unique_id_t::type id;

        MSGPACK_DEFINE(id);
    };
}

class native_server_t;

class native_server_job_t:
    public unique_id_t,
    public job::job_t
{
    public:
        native_server_job_t(const messages::request_t& request,
                            native_server_t* driver,
                            const networking::route_t& route);

        virtual void react(const events::response& event);
        virtual void react(const events::error& event);
        virtual void react(const events::completed& event);

    private:
        template<class T>
        void send(const T& response) {
            zmq::message_t message;
            zeromq_server_t* server = static_cast<zeromq_server_t*>(m_driver);

            // Send the identity
            for(networking::route_t::const_iterator id = m_route.begin(); id != m_route.end(); ++id) {
                message.rebuild(id->size());
                memcpy(message.data(), id->data(), id->size());
                server->socket().send(message, ZMQ_SNDMORE);
            }

            // Send the delimiter
            message.rebuild(0);
            server->socket().send(message, ZMQ_SNDMORE);

            // Send the response
            server->socket().send_multi(
                boost::tie(
                    T::message_code,
                    response
                )
            );
        }

    private:
        const networking::route_t m_route;
};

class native_server_t:
    public zeromq_server_t
{
    public:
        native_server_t(engine_t* engine,
                        const std::string& method, 
                        const Json::Value& args);

        // Driver interface
        virtual Json::Value info() const;
        
    private:
        // Server interface
        virtual void process(ev::idle&, int);
};

}}}

#endif
