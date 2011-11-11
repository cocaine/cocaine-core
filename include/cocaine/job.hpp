#ifndef COCAINE_JOB_HPP
#define COCAINE_JOB_HPP

#include <boost/enable_shared_from_this.hpp>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"
#include "cocaine/lines.hpp"

namespace cocaine { namespace engine {

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
        job_t(driver_t* parent,
              ev::tstamp deadline = 0.,
              bool urgent = false);

        virtual void enqueue();

        virtual void send(zmq::message_t& chunk) = 0; 
        virtual void send(error_code code, const std::string& error) = 0;
        
        void audit(ev::tstamp spent);
        
        inline bool urgent() const {
            return m_urgent;
        }

    private:
        void timeout(ev::periodic&, int);

    protected:
        driver_t* m_parent;

    private:
        bool m_urgent;
        ev::periodic m_expiration_timer;
};

class publication_t:
    public job_t
{
    public:
        publication_t(driver_t* parent);

        virtual void send(zmq::message_t& chunk);
        virtual void send(error_code code, const std::string& error);
};

}}

#endif
