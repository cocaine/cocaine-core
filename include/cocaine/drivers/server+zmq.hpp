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
                       const lines::route_t& route,
                       zmq_server_t* server);

    public:
        virtual void send(zmq::message_t& chunk);

    private:
        const lines::route_t m_route;
        zmq_server_t* m_server;
};

class zmq_server_t:
    public driver_t
{
    public:
        zmq_server_t(engine_t* engine,
                     const std::string& method, 
                     const Json::Value& args);
        virtual ~zmq_server_t();

    public:
        // Driver interface
        virtual void pause();
        virtual void resume();
        virtual Json::Value info() const;
        
        // Server interface
        void operator()(ev::io&, int);
        virtual void process(ev::idle&, int);

        void respond(const lines::route_t& route, zmq::message_t& chunk);

    protected:
        lines::socket_t m_socket;
        ev::io m_watcher; 
        ev::idle m_processor;
};

}}}

#endif
