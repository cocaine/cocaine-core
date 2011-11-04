#ifndef COCAINE_DRIVERS_ZMQ_SERVER_HPP
#define COCAINE_DRIVERS_ZMQ_SERVER_HPP

#include "cocaine/deferred.hpp"
#include "cocaine/drivers/abstract.hpp"

namespace cocaine { namespace engine { namespace drivers {

class zmq_server_t;

class zmq_response_t:
    public lines::deferred_t
{
    public:
        zmq_response_t(const std::string& method,
                       zmq_server_t* server,
                       const lines::route_t& route);

    public:
        virtual void enqueue(engine_t* engine);
        virtual void send(zmq::message_t& chunk);
        virtual void abort(const std::string& error);

    public:
        const lines::route_t& route();
        zmq::message_t& request();

    protected:
        zmq_server_t* m_server;
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

    public:
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
