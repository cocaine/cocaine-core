#ifndef COCAINE_DRIVER_ZEROMQ_SERVER_HPP
#define COCAINE_DRIVER_ZEROMQ_SERVER_HPP

#include "cocaine/drivers/base.hpp"
#include "cocaine/job.hpp"
#include "cocaine/networking.hpp"

namespace cocaine { namespace engine { namespace driver {

class zeromq_server_t;

class zeromq_server_job_t:
    public job::job_t
{
    public:
        zeromq_server_job_t(zeromq_server_t* driver, 
                            const networking::route_t& route);

        virtual void react(const events::chunk_t& event);
        virtual void react(const events::error_t& event);

    private:
        bool send(zmq::message_t& chunk);

    protected:
        const networking::route_t m_route;
};

class zeromq_server_t:
    public driver_t
{
    public:
        zeromq_server_t(engine_t* engine,
                        const std::string& method, 
                        const Json::Value& args);
        virtual ~zeromq_server_t();

        // Driver interface
        virtual Json::Value info() const;

        // Job interface
        inline networking::channel_t& socket() {
            return m_socket;
        }

    private:
        void event(ev::io&, int);
        
        // Server interface
        virtual void process(ev::idle&, int);

    protected:
        networking::channel_t m_socket;
        
        ev::io m_watcher; 
        ev::idle m_processor;
};

}}}

#endif
