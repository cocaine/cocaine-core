#ifndef COCAINE_DRIVER_RECURRING_TIMER_HPP
#define COCAINE_DRIVER_RECURRING_TIMER_HPP

#include "cocaine/drivers/timer_base.hpp"

namespace cocaine { namespace engine { namespace driver {

class recurring_timer_t:
    public timer_base_t<recurring_timer_t>
{
    public:
        recurring_timer_t(engine_t* engine,
                          const std::string& method, 
                          const Json::Value& args);

        // Driver interface
        virtual Json::Value info() const;

        // Timer interface
        ev::tstamp reschedule(ev::tstamp now);

    private:
        const ev::tstamp m_interval;
};

}}}

#endif
