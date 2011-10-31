#ifndef COCAINE_DRIVERS_AUTO_HPP
#define COCAINE_DRIVERS_AUTO_HPP

#include "cocaine/drivers/timed.hpp"

namespace cocaine { namespace engine { namespace drivers {

class auto_t:
    public timed_driver_t<auto_t>
{
    public:
        auto_t(const std::string& method,
               engine_t* engine, 
               const Json::Value& args);

    public:       
        virtual Json::Value info() const;

        ev::tstamp reschedule(ev::tstamp now);

    private:
        const ev::tstamp m_interval;
};

}}}

#endif
