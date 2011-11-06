#ifndef COCAINE_JOB_HPP
#define COCAINE_JOB_HPP

#include <boost/enable_shared_from_this.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/lines.hpp"

namespace cocaine { namespace engine {

class job_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<job_t>,
    public birth_control_t<job_t>
{
    public:
        job_t(driver_t* parent);

        virtual void enqueue();
        virtual void respond(zmq::message_t& chunk) = 0; 
        virtual void abort(error_code code, const std::string& error) = 0;
        
        void audit(ev::tstamp spent);

    protected:
        driver_t* m_parent;
};

class publication_t:
    public job_t
{
    public:
        publication_t(driver_t* parent);

        virtual void respond(zmq::message_t& chunk);
        virtual void abort(error_code code, const std::string& error);
};

}}

#endif
