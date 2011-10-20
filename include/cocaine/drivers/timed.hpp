#ifndef COCAINE_DRIVERS_TIMED_HPP
#define COCAINE_DRIVERS_TIMED_HPP

#include "cocaine/drivers/base.hpp"

namespace cocaine { namespace engine { namespace drivers {

template<class T>
class timed_driver_base_t:
    public driver_base_t< ev::periodic, timed_driver_base_t<T> >
{
    public:
        inline void initialize() {
            ev_periodic_set(static_cast<ev_periodic*>(this->m_watcher.get()), 0, 0, thunk);
        }

    protected:
        timed_driver_base_t(const std::string& name, boost::shared_ptr<engine_t> parent):
            driver_base_t<ev::periodic, timed_driver_base_t>(name, parent)
        {}
    
    private:
        static ev::tstamp thunk(ev_periodic* w, ev::tstamp now) {
            return static_cast<T*>(w->data)->reschedule(now);
        }
};

}}}

#endif
