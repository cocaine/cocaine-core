#ifndef COCAINE_DRIVERS_AUTO_HPP
#define COCAINE_DRIVERS_AUTO_HPP

#include <boost/lexical_cast.hpp>

#include "cocaine/drivers/timed.hpp"

namespace cocaine { namespace engine { namespace drivers {

class auto_t:
    public timed_driver_base_t<auto_t>
{
    public:
        auto_t(const std::string& name, boost::shared_ptr<engine_t> parent, const Json::Value& args):
            timed_driver_base_t<auto_t>(name, parent, args),
            m_interval(args.get("interval", 0).asInt() / 1000.0)
        {
            if(m_interval <= 0) {
                throw std::runtime_error("no interval specified");
            }

            m_id = "auto:" + digest_t().get(
                m_name +
                boost::lexical_cast<std::string>(m_interval));
        }
       
        inline ev::tstamp reschedule(ev::tstamp now) {
            return now + m_interval;
        }

    private:
        ev::tstamp m_interval;
};

}}}

#endif
