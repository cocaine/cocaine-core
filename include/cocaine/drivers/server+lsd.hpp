#ifndef COCAINE_DRIVERS_LSD_SERVER_HPP
#define COCAINE_DRIVERS_LSD_SERVER_HPP

#include "cocaine/deferred.hpp"
#include "cocaine/drivers/server+zmq.hpp"

namespace cocaine { namespace engine { namespace drivers {

class lsd_server_t;

class lsd_response_t:
    public zmq_response_t
{
    public:
        lsd_response_t(lsd_server_t* server, const lines::route_t& route);

        virtual void abort(error_code code, const std::string& error);
        
    public:
        zmq::message_t& envelope();

    private:
        zmq::message_t m_envelope;
};

class lsd_server_t:
    public zmq_server_t
{
    public:
        lsd_server_t(engine_t* engine,
                     const std::string& method, 
                     const Json::Value& args);

        // Driver interface
        virtual Json::Value info() const;

        // Server interface
        virtual void process(ev::idle&, int);
        virtual void respond(zmq_response_t* response, zmq::message_t& chunk);
};

}}}

#endif
