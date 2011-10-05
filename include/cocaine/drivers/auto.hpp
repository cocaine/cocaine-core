#ifndef COCAINE_DRIVERS_AUTO_HPP
#define COCAINE_DRIVERS_AUTO_HPP

#include <boost/lexical_cast.hpp>

#include "cocaine/drivers/timed.hpp"

namespace cocaine { namespace engine { namespace drivers {

class auto_t:
    public timed_driver_base_t<auto_t>
{
    public:
        auto_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source, const Json::Value& args):
            timed_driver_base_t<auto_t>(parent, source),
            m_interval(args.get("interval", 0).asInt() / 1000.0)
        {
            if(~m_source->capabilities() & plugin::source_t::ITERATOR) {
                throw std::runtime_error("source doesn't support iteration");
            }
            
            if(m_interval <= 0) {
                throw std::runtime_error("no interval specified");
            }

            m_id = "auto:" + digest_t().get(m_source->uri() + 
                boost::lexical_cast<std::string>(m_interval));
        
            syslog(LOG_DEBUG, "thread %s in %s: driver %s is starting with %.02fs intervals",
                m_parent->id().c_str(), m_source->uri().c_str(), m_id.c_str(), m_interval);
        }
       
        inline ev::tstamp reschedule(ev::tstamp now) {
            return now + m_interval;
        }

    private:
        ev::tstamp m_interval;
};

}}}

#endif
