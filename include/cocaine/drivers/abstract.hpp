#ifndef COCAINE_DRIVERS_ABSTRACT_HPP
#define COCAINE_DRIVERS_ABSTRACT_HPP

#include "cocaine/common.hpp"

namespace cocaine { namespace engine { namespace drivers {

class abstract_driver_t:
    public boost::noncopyable
{
    public:
        virtual ~abstract_driver_t() {
            syslog(LOG_DEBUG, "driver %s: terminating", m_id.c_str());
        }

        inline std::string id() const {
            return m_id;
        }

    protected:
        abstract_driver_t(const std::string& name):
            m_name(name)
        {}
        
    protected:
        std::string m_id;
        std::string m_name;
};

}}}

#endif
