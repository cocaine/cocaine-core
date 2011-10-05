#ifndef COCAINE_DRIVERS_MANUAL_HPP
#define COCAINE_DRIVERS_MANUAL_HPP

#include "cocaine/drivers/timed.hpp"

namespace cocaine { namespace engine { namespace drivers {

class manual_t:
    public timed_driver_base_t<manual_t>
{
    public:
        manual_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            timed_driver_base_t<manual_t>(parent, source)
        {
            if(~m_source->capabilities() & plugin::source_t::SCHEDULER) {
                throw std::runtime_error("source doesn't support manual scheduling");
            }
            
            m_id = "manual:" + digest_t().get(m_source->uri());
            
            syslog(LOG_DEBUG, "thread %s in %s: driver %s is starting",
                m_parent->id().c_str(), m_source->uri().c_str(), m_id.c_str());
        }

        inline ev::tstamp reschedule(ev::tstamp now) {
            float next = m_source->reschedule();
            return now >= next ? now : next;
        }
};

}}}

#endif
