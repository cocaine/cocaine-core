#ifndef COCAINE_DRIVERS_TIMED_HPP
#define COCAINE_DRIVERS_TIMED_HPP

#include "cocaine/drivers/abstract.hpp"

namespace cocaine { namespace engine { namespace drivers {

template<class T>
class timed_driver_t:
    public driver_t
{
    public:
        timed_driver_t(const std::string& method, boost::shared_ptr<engine_t> parent):
            driver_t(method, parent)
        {
            m_watcher.set(this);
            ev_periodic_set(static_cast<ev_periodic*>(&m_watcher), 0, 0, thunk);
            m_watcher.start();
        }

    public: 
        virtual void pause() {
            m_watcher.stop();
        }

        virtual void resume() {
            m_watcher.start();
        }

        void operator()(ev::periodic&, int) {
            boost::shared_ptr<lines::publication_t> deferred(
                new lines::publication_t(m_method, m_parent));

            try {
                m_parent->queue(
                    deferred,
                    boost::make_tuple(
                        INVOKE,
                        m_method));
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "driver [%s:%s]: failed to enqueue the invocation - %s",
                    m_parent->name().c_str(), m_method.c_str(), e.what());
                deferred->abort(e.what());
            }
        }
    private:
        static ev::tstamp thunk(ev_periodic* w, ev::tstamp now) {
            return static_cast<T*>(w->data)->reschedule(now);
        }

    private:
        ev::periodic m_watcher;
};

}}}

#endif
