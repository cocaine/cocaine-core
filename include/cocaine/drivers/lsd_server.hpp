#ifndef COCAINE_DRIVER_LSD_SERVER_HPP
#define COCAINE_DRIVER_LSD_SERVER_HPP

#include "cocaine/drivers/zeromq_server.hpp"

namespace cocaine { namespace engine { namespace driver {

class lsd_server_t;

class lsd_job_t:
    public unique_id_t,
    public job::job_t
{
    public:
        lsd_job_t(lsd_server_t* driver,
                  job::policy_t policy,
                  const unique_id_t::type& id,
                  const networking::route_t& route);

        virtual void react(const events::response& event);
        virtual void react(const events::error& event);
        virtual void react(const events::completed& event);

    private:
        bool send(const Json::Value& envelope, int flags = 0);

    private:
        const networking::route_t m_route;
};

class lsd_server_t:
    public zeromq_server_t
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
