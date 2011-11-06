#ifndef COCAINE_BACKENDS_ABSTRACT_HPP
#define COCAINE_BACKENDS_ABSTRACT_HPP

#include <queue>

#include "cocaine/common.hpp"
#include "cocaine/forwards.hpp"

namespace cocaine { namespace engine {

class backend_t:
    public boost::noncopyable,
    public unique_id_t,
    public birth_control_t<backend_t>
{
    public:
        typedef std::queue<
            boost::shared_ptr<deferred_t>
        > deferred_queue_t;

    public:       
        backend_t(engine_t* engine);
        virtual ~backend_t();

        void rearm(float timeout);
        
    public:
        bool active() const;

        deferred_queue_t& queue();
        const deferred_queue_t& queue() const;

    protected:
        virtual void kill() = 0;

    private:
        void timeout(ev::timer&, int);

    protected:
        engine_t* m_engine;

    private:
        bool m_active;
        ev::timer m_heartbeat;

        deferred_queue_t m_queue;
};

}}

#endif
