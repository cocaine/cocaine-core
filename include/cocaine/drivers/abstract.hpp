#ifndef COCAINE_DRIVERS_ABSTRACT_HPP
#define COCAINE_DRIVERS_ABSTRACT_HPP

#include "cocaine/common.hpp"
#include "cocaine/engine.hpp"

namespace cocaine { namespace engine {

class driver_t:
    public boost::noncopyable
{
    public:
        driver_t(engine_t* engine, const std::string& method):
            m_engine(engine),
            m_method(method),
            m_spent(0)
        {
            syslog(LOG_DEBUG, "driver [%s:%s]: constructing", 
                m_engine->name().c_str(), m_method.c_str());
        }
        
        virtual ~driver_t() {
            syslog(LOG_DEBUG, "driver [%s:%s]: destructing",
                m_engine->name().c_str(), m_method.c_str());
        }

        inline engine_t* engine() { 
            return m_engine; 
        }

        inline const std::string& method() const { 
            return m_method; 
        }
        
        inline void audit(ev::tstamp spent) {
            m_spent += spent;
        }

        virtual void expire(boost::shared_ptr<job_t> job) {
            m_engine->expire(job);
        }
        
        virtual Json::Value info() const = 0;

    protected:
        // Bindings
        engine_t* m_engine;
        const std::string m_method;

        // Audit
        ev::tstamp m_spent;
};

}}

#endif
