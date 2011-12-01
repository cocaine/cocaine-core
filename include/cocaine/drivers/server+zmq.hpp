#ifndef COCAINE_DRIVER_ZMQ_SERVER_HPP
#define COCAINE_DRIVER_ZMQ_SERVER_HPP

#include "cocaine/drivers/base.hpp"
#include "cocaine/job.hpp"
#include "cocaine/lines.hpp"

namespace cocaine { namespace engine { namespace driver {

class zmq_server_t;

class zmq_job_t:
    public job::job_t
{
    public:
        zmq_job_t(zmq_server_t* driver, 
                  job::policy_t policy,
                  const lines::route_t& route);

        virtual void react(const events::response& event);
        virtual void react(const events::error& event);

    private:
        void send(zmq::message_t& chunk);

    protected:
        const lines::route_t m_route;
};

class zmq_server_t:
    public driver_t
{
    public:
        zmq_server_t(engine_t* engine,
                     const std::string& method, 
                     const Json::Value& args);

        // Driver interface
        virtual void stop();

        virtual Json::Value info() const;

        // Job interface
        inline lines::socket_t& socket() {
            return m_socket;
        }

    private:
        void event(ev::io&, int);
        
        // Server interface
        virtual void process(ev::idle&, int);

    protected:
        lines::socket_t m_socket;
        
        ev::io m_watcher; 
        ev::idle m_processor;
};

}}}

#endif
