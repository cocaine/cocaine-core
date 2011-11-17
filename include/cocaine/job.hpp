#ifndef COCAINE_JOB_HPP
#define COCAINE_JOB_HPP

#include <boost/enable_shared_from_this.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/lines.hpp"

namespace cocaine { namespace engine {

struct job_policy {
    public:
        static job_policy defaults();

    public:
        bool urgent;
        ev::tstamp timeout;
        ev::tstamp deadline;
};

enum job_state {
    queued,
    running
};

class job_t:
    public boost::noncopyable,
    public boost::enable_shared_from_this<job_t>,
    public birth_control_t<job_t>
{
    public:
        job_t(driver_t* parent, job_policy policy = job_policy::defaults());

        virtual void enqueue();
        virtual void send(zmq::message_t& chunk) = 0; 
        virtual void send(error_code code, const std::string& error) = 0;
        
        void audit(ev::tstamp spent);
        
        inline job_policy& policy() {
            return m_policy;
        }

    private:
        void expire(ev::periodic&, int);

    protected:
        driver_t* m_parent;

    private:
        job_policy m_policy;
        ev::periodic m_expiration_timer;
};

class publication_t:
    public job_t
{
    public:
        publication_t(driver_t* parent, job_policy policy = job_policy::defaults());

        virtual void send(zmq::message_t& chunk);
        virtual void send(error_code code, const std::string& error);
};

}}

#endif
