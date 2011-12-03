#ifndef COCAINE_EVENTS_HPP
#define COCAINE_EVENTS_HPP

#include <boost/statechart/event.hpp>

#include "cocaine/forwards.hpp"
#include <cocaine/networking.hpp>

namespace cocaine { namespace engine { namespace events {

namespace sc = boost::statechart;

// Common events
struct enqueued:
    public sc::event<enqueued>
{
    enqueued(size_t position_):
        position(position_)
    { }

    const size_t position;
};

struct invoked:
    public sc::event<invoked>
{
    invoked(const boost::shared_ptr<job::job_t> job_):
        job(job_)
    { }

    const boost::shared_ptr<job::job_t> job;
};

struct completed:
    public sc::event<completed>
{ };

// Slave events
struct heartbeat:
    public sc::event<heartbeat>
{ };

struct death:
    public sc::event<death>
{ };

// Job events
struct response:
    public sc::event<response>
{
    response(zmq::message_t& message_):
        message(message_)
    { }

    zmq::message_t& message;
};

// Errors
struct error {
    public:
        const unsigned int code;
        const std::string& message;
    
    protected:
        error(unsigned int code_, const std::string& message_):
            code(code_),
            message(message_)
        { }
};

struct request_error:
    public sc::event<request_error>,
    public error
{
    request_error(const std::string& message):
        error(400, message)
    { }
};

struct server_error:
    public sc::event<server_error>,
    public error
{
    server_error(const std::string& message):
        error(500, message)
    { }
};

struct application_error:
    public sc::event<application_error>,
    public error
{
    application_error(const std::string& message):
        error(502, message)
    { }
};

struct resource_error:
    public sc::event<resource_error>,
    public error
{
    resource_error(const std::string& message):
        error(503, message)
    { }
};

struct timeout_error:
    public sc::event<timeout_error>,
    public error
{
    timeout_error(const std::string& message):
        error(504, message)
    { }
};

}}}

#endif
