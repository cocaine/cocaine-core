#ifndef COCAINE_BACKENDS_ABSTRACT_HPP
#define COCAINE_BACKENDS_ABSTRACT_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"

namespace cocaine { namespace engine {

enum state_t {
    inactive,
    active
};

class backend_t:
    public boost::noncopyable,
    public unique_id_t,
    public birth_control_t<backend_t>
{
    public:       
        backend_t(engine_t* engine);
        virtual ~backend_t();

        void rearm(float timeout);
        
    public:
        state_t state() const;

        boost::shared_ptr<job_t>& job();
        const boost::shared_ptr<job_t>& job() const;

    protected:
        virtual void kill() = 0;

    private:
        void timeout(ev::timer&, int);

    protected:
        engine_t* m_engine;

    private:
        state_t m_state;
        boost::shared_ptr<job_t> m_job;

        ev::timer m_heartbeat;
};

}}

#endif
