#ifndef COCAINE_DRIVERS_SERVER_HPP
#define COCAINE_DRIVERS_SERVER_HPP

#include "cocaine/deferred.hpp"
#include "cocaine/drivers/abstract.hpp"

namespace cocaine { namespace engine { namespace drivers {

class server_t;

class response_t:
    public lines::deferred_t
{
    public:
        response_t(const lines::route_t& route, server_t* server);

    public:
        virtual void send(zmq::message_t& chunk);

    private:
        const lines::route_t m_route;
        server_t* m_server;
};

class server_t:
    public driver_t
{
    public:
        server_t(const std::string& method, 
                 engine_t* engine,
                 const Json::Value& args);
        virtual ~server_t();

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
