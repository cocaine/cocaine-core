#ifndef COCAINE_BACKENDS_ABSTRACT_HPP
#define COCAINE_BACKENDS_ABSTRACT_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"

namespace cocaine { namespace engine {

enum backend_state_t {
    inactive,
    idle,
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

        backend_state_t state() const;
        
        inline boost::shared_ptr<job_t>& job() {
            return m_job;
        }

    protected:
        virtual void kill() = 0;

    private:
        void timeout(ev::timer&, int);

    protected:
        engine_t* m_engine;

    private:
        bool m_settled;
        ev::timer m_heartbeat_timer;
        boost::shared_ptr<job_t> m_job;
};

}}

#endif
