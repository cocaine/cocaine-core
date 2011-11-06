#ifndef COCAINE_DRIVERS_ZMQ_SERVER_HPP
#define COCAINE_DRIVERS_ZMQ_SERVER_HPP

#include "cocaine/deferred.hpp"
#include "cocaine/drivers/abstract.hpp"

namespace cocaine { namespace engine { namespace drivers {

class zmq_server_t;

class zmq_response_t:
    public deferred_t
{
    public:
        zmq_response_t(zmq_server_t* server, const lines::route_t& route);

        virtual void enqueue();
        virtual void respond(zmq::message_t& chunk);
        virtual void abort(error_code code, const std::string& error);

    public:
        const lines::route_t& route();
        zmq::message_t& request();

    protected:
        const lines::route_t m_route;
        zmq::message_t m_request;
};

class zmq_server_t:
    public driver_t
{
    public:
        zmq_server_t(engine_t* engine,
                     const std::string& method, 
                     const Json::Value& args);

        // Driver interface
        virtual void suspend();
        virtual void resume();
        virtual Json::Value info() const;
        
        // Server interface
        void operator()(ev::io&, int);
        
        virtual void process(ev::idle&, int);
        virtual void respond(zmq_response_t* response, zmq::message_t& chunk);

    protected:
        lines::socket_t m_socket;
        ev::io m_watcher; 
        ev::idle m_processor;
};

}}}

#endif
