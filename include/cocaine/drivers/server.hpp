#ifndef COCAINE_DRIVERS_SERVER_HPP
#define COCAINE_DRIVERS_SERVER_HPP

#include "cocaine/deferred.hpp"
#include "cocaine/drivers/abstract.hpp"

namespace cocaine { namespace engine { namespace drivers {

class responder_t {
    public:
        virtual void respond(const lines::route_t& route, zmq::message_t& chunk) = 0;
};

class response_t:
    public lines::deferred_t
{
    public:
        response_t(const lines::route_t& route, responder_t* responder);

    public:
        virtual void send(zmq::message_t& chunk);

    private:
        const lines::route_t m_route;
        responder_t* m_responder;
};

class server_t:
    public driver_t,
    public responder_t
{
    public:
        server_t(const std::string& method, 
                 engine_t* engine,
                 const Json::Value& args);
        virtual ~server_t();

    public:
        virtual void pause();
        virtual void resume();

        virtual Json::Value info() const;
        
        virtual void respond(const lines::route_t& route, zmq::message_t& chunk);

        void operator()(ev::io&, int);
        void process(ev::idle&, int);

    private:
        lines::socket_t m_socket;
        ev::io m_watcher; 
        ev::idle m_processor;
};

}}}

#endif
