#ifndef COCAINE_DRIVER_LSD_SERVER_HPP
#define COCAINE_DRIVER_LSD_SERVER_HPP

#include "cocaine/drivers/server+zmq.hpp"

namespace cocaine { namespace engine { namespace driver {

class lsd_server_t;

class lsd_job_t:
    public zmq_job_t,
    public unique_id_t
{
    public:
        lsd_job_t(lsd_server_t* driver,
                  job::policy_t policy,
                  const lines::route_t& route,
                  const unique_id_t::type& id);

        virtual void react(const events::response& event);
        virtual void react(const events::error& event);
        virtual void react(const events::exemption& event);

    private:
        void send(const Json::Value& envelope, zmq::message_t& chunk);
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
        
    private:
        // Server interface
        virtual void process(ev::idle&, int);
};

}}}

#endif
