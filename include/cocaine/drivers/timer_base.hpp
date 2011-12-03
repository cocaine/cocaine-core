#ifndef COCAINE_DRIVER_TIMER_BASE_HPP
#define COCAINE_DRIVER_TIMER_BASE_HPP

#include "cocaine/drivers/base.hpp"
#include "cocaine/engine.hpp"

namespace cocaine { namespace engine { namespace driver {

template<class T>
class timer_base_t:
    public driver_t
{
    public:
        timer_base_t(engine_t* engine, const std::string& method):
            driver_t(engine, method)
        {
            m_watcher.set<timer_base_t, &timer_base_t::event>(this);
            ev_periodic_set(static_cast<ev_periodic*>(&m_watcher), 0, 0, thunk);
            m_watcher.start();
        }

        virtual ~timer_base_t() {
            m_watcher.stop();
        }

    private:
        static ev::tstamp thunk(ev_periodic* w, ev::tstamp now) {
            return static_cast<T*>(w->data)->reschedule(now);
        }

    private:
        void event(ev::periodic&, int) {
            boost::shared_ptr<publication_t> job(new publication_t(this));
            m_engine->enqueue(job);
        }
        
    private:
        ev::periodic m_watcher;
};

}}}

#endif
