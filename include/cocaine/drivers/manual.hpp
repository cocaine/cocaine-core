#ifndef COCAINE_DRIVERS_MANUAL_HPP
#define COCAINE_DRIVERS_MANUAL_HPP

#include "cocaine/drivers/timed.hpp"

namespace cocaine { namespace engine { namespace drivers {

class manual_t:
    public timed_driver_base_t<manual_t>
{
    public:
        manual_t(const std::string& name, boost::shared_ptr<overseer_t> parent, const Json::Value& args):
            timed_driver_base_t<manual_t>(name, parent, args)
        {
            if(~m_parent->capabilities() & plugin::source_t::SCHEDULER) {
                throw std::runtime_error("source doesn't support manual scheduling");
            }
            
            m_id = "manual:" + security::digest_t().get(m_name);
        }

        inline ev::tstamp reschedule(ev::tstamp now) {
        
        }
};

}}}

#endif
