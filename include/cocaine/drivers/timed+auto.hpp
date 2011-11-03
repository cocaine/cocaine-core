#ifndef COCAINE_DRIVERS_AUTO_TIMED_HPP
#define COCAINE_DRIVERS_AUTO_TIMED_HPP

#include "cocaine/drivers/timed.hpp"

namespace cocaine { namespace engine { namespace drivers {

class auto_timed_t:
    public timed_t<auto_timed_t>
{
    public:
        auto_timed_t(engine_t* engine,
                     const std::string& method, 
                     const Json::Value& args);

    public:       
        virtual Json::Value info() const;

        ev::tstamp reschedule(ev::tstamp now);

    private:
        const ev::tstamp m_interval;
};

}}}

#endif
