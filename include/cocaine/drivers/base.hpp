#ifndef COCAINE_DRIVERS_BASE_HPP
#define COCAINE_DRIVERS_BASE_HPP

#include "cocaine/drivers/abstract.hpp"

namespace cocaine { namespace engine { namespace drivers {

template<class WatcherType, class DriverType>
class driver_base_t:
    public abstract_driver_t
{
    public:
        driver_base_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source):
            abstract_driver_t(parent, source)    
        {}
        
        virtual ~driver_base_t() {
            if(m_watcher.get() && m_watcher->is_active()) {
                m_watcher->stop();
            }
        }

        void start() {
            syslog(LOG_DEBUG, "thread %s in %s: driver %s is starting",
                m_parent->id().c_str(), m_source->uri().c_str(), m_id.c_str());
            
            m_watcher.reset(new WatcherType(m_parent->loop()));
            m_watcher->set(this);

            static_cast<DriverType*>(this)->initialize();

            m_watcher->start();
        }

        inline void stop() {
            m_parent->reap(m_id);
        }

        virtual void operator()(WatcherType&, int) {
            Json::Value result;    
            
            try {
                result = m_source->invoke();
            } catch(const std::exception& e) {
                syslog(LOG_ERR, "driver %s in thread %s in %s: [%s()] %s",
                    m_id.c_str(), m_parent->id().c_str(),
                    m_source->uri().c_str(), __func__, e.what());
                result["error"] = e.what();
            }
            
            publish(result);
        }
    
    protected:
        // Watcher
        std::auto_ptr<WatcherType> m_watcher;
};

}}}

#endif
