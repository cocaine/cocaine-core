#ifndef COCAINE_BACKENDS_ABSTRACT_HPP
#define COCAINE_BACKENDS_ABSTRACT_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"

namespace cocaine { namespace engine {

enum backend_state {
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

        virtual void stop() = 0;

        void rearm();

    public:        
        inline boost::shared_ptr<job_t>& job() {
            return m_job;
        }
        
        backend_state state() const;
        
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
