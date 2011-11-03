#ifndef COCAINE_DRIVERS_LSD_SERVER_HPP
#define COCAINE_DRIVERS_LSD_SERVER_HPP

#include "cocaine/deferred.hpp"
#include "cocaine/drivers/server+zmq.hpp"

namespace cocaine { namespace engine { namespace drivers {

class lsd_server_t;

class lsd_response_t:
    public lines::deferred_t
{
    public:
        lsd_response_t(const std::string& method,
                       lsd_server_t* server,
                       const lines::route_t& route);

    public:
        virtual void enqueue(engine_t* engine);
        virtual void send(zmq::message_t& chunk);
        virtual void abort(const std::string& error);

    public:
        zmq::message_t& request();
        zmq::message_t& envelope();

    private:
        lsd_server_t* m_server;
        const lines::route_t m_route;
        
        zmq::message_t m_request, m_envelope;
};

class lsd_server_t:
    public zmq_server_t
{
    public:
        lsd_server_t(engine_t* engine,
                     const std::string& method, 
                     const Json::Value& args);

    public:
        virtual Json::Value info() const;
        virtual void process(ev::idle&, int);

        void respond(const lines::route_t& route, 
                     zmq::message_t& envelope, 
                     zmq::message_t& chunk);
};

}}}

#endif
