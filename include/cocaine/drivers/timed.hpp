#ifndef COCAINE_DRIVERS_TIMED_HPP
#define COCAINE_DRIVERS_TIMED_HPP

#include "cocaine/drivers/base.hpp"

namespace cocaine { namespace engine { namespace drivers {

template<class TimedDriverType>
class timed_driver_base_t:
    public driver_base_t<ev::periodic, timed_driver_base_t<TimedDriverType> >
{
    public:
        timed_driver_base_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source):
            driver_base_t<ev::periodic, timed_driver_base_t>(parent, source)
        {}

        inline void initialize() {
            ev_periodic_set(static_cast<ev_periodic*>(this->m_watcher.get()), 0, 0, thunk);
        }

    private:
        inline ev::tstamp reschedule(ev::tstamp now) {
            return static_cast<Type*>(this)->reschedule(now);
        }

        typedef TimedDriverType Type;

        static ev::tstamp thunk(ev_periodic* w, ev::tstamp now) {
            timed_driver_base_t<Type>* driver = static_cast<timed_driver_base_t<Type>*>(w->data);

            try {
                return driver->reschedule(now);
            } catch(const std::exception& e) {
                syslog(LOG_ERR, "engine: %s driver is broken - %s",
                    driver->id().c_str(), e.what());
                driver->stop();
                return now;
            }
        }
};

}}}

#endif
