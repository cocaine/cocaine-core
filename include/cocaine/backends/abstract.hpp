#ifndef COCAINE_BACKENDS_ABSTRACT_HPP
#define COCAINE_BACKENDS_ABSTRACT_HPP

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"

namespace cocaine { namespace engine {

enum state_t {
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

        state_t state() const;

        void rearm();
        
        void assign(boost::shared_ptr<job_t> job);
        void resign();

        boost::shared_ptr<job_t> job();

    protected:
        virtual void kill() = 0;

    private:
        void timeout(ev::timer&, int);

    protected:
        engine_t* m_engine;

    private:
        bool m_settled;
        ev::timer m_heartbeat;
        boost::shared_ptr<job_t> m_job;
};

}}

#endif
