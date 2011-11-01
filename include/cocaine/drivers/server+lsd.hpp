#ifndef COCAINE_DRIVERS_SERVER_LSD_HPP
#define COCAINE_DRIVERS_SERVER_LSD_HPP

#include "cocaine/deferred.hpp"
#include "cocaine/drivers/server.hpp"

namespace cocaine { namespace engine { namespace drivers {

class lsd_server_t;

class lsd_response_t:
    public lines::deferred_t
{
    public:
        lsd_response_t(const lines::route_t& route, lsd_server_t* server);

    public:
        // Deferred interace
        virtual void send(zmq::message_t& chunk);

        // LSD support
        zmq::message_t& envelope();

    private:
        const lines::route_t m_route;
        zmq::message_t m_envelope;
        lsd_server_t* m_server;
};

class lsd_server_t:
    public server_t
{
    public:
        lsd_server_t(const std::string& method, 
                     engine_t* engine,
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
