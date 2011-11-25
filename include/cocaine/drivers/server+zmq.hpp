#ifndef COCAINE_DRIVERS_ZMQ_SERVER_HPP
#define COCAINE_DRIVERS_ZMQ_SERVER_HPP

#include "cocaine/drivers/abstract.hpp"
#include "cocaine/job.hpp"
#include "cocaine/lines.hpp"

namespace cocaine { namespace engine { namespace drivers {

class zmq_server_t;

class zmq_job_t:
    public job_t
{
    public:
        zmq_job_t(zmq_server_t* server, const lines::route_t& route);

        virtual job_state enqueue();

        virtual void send(zmq::message_t& chunk);
        virtual void send(error_code code, const std::string& error);

    public:
        inline const lines::route_t& route() const {
            return m_route;
        }

        inline zmq::message_t& request() {
            return m_request;
        }

    protected:
        const lines::route_t m_route;
        zmq::message_t m_request;
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

        virtual void send(zmq_job_t* job, zmq::message_t& chunk);
    
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
