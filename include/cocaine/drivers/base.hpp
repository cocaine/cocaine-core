#ifndef COCAINE_DRIVERS_BASE_HPP
#define COCAINE_DRIVERS_BASE_HPP

#include "cocaine/drivers/abstract.hpp"
#include "cocaine/plugin.hpp"
#include "cocaine/security/digest.hpp"

namespace cocaine { namespace engine { namespace drivers {

using namespace cocaine::security;

template<class WatcherType, class DriverType>
class driver_base_t:
    public abstract_driver_t
{
    public:
        driver_base_t(const std::string& name, boost::shared_ptr<engine_t> parent, const Json::Value& args):
            abstract_driver_t(name, parent),
            m_timeout(args.get("timeout", config_t::get().engine.heartbeat_timeout).asDouble())
        { }
        
        virtual ~driver_base_t() {
            if(m_watcher.get() && m_watcher->is_active()) {
                m_watcher->stop();
            }
        }

        void start() {
            syslog(LOG_DEBUG, "driver %s [%s]: starting",
                m_id.c_str(), m_parent->id().c_str());
            
            m_watcher.reset(new WatcherType());
            m_watcher->set(this);

            static_cast<DriverType*>(this)->initialize();

            m_watcher->start();
        }

        virtual void operator()(WatcherType&, int) {
            try {
                m_parent->queue(
                    boost::make_tuple(
                        INVOKE,
                        m_name)
                );
            } catch(const std::runtime_error& e) {
                syslog(LOG_ERR, "driver %s [%s]: %s",
                    m_id.c_str(), m_parent->id().c_str(), e.what());
            }
        }
    
    protected:
        float m_timeout;

        // Watcher
        std::auto_ptr<WatcherType> m_watcher;
};

}}}

#endif
