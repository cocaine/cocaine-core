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
            boost::shared_ptr<lines::deferred_t>
        > deferred_queue_t;

    public:       
        backend_t();
        virtual ~backend_t();

    public:
        void rearm(float timeout);
        
        bool active() const;

        deferred_queue_t& queue();
        const deferred_queue_t& queue() const;

    protected:
        virtual void timeout(ev::timer& w, int revents) = 0;

    private:
        bool m_active;
        ev::timer m_heartbeat;
        deferred_queue_t m_queue;
};

}}

#endif
