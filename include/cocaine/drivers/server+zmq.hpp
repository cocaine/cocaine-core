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
        virtual void send(zmq::message_t& chunk);
        virtual void abort(const std::string& error);

    private:
        zmq_server_t* m_server;
        const lines::route_t m_route;
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
