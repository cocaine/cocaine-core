#ifndef COCAINE_DRIVERS_ABSTRACT_HPP
#define COCAINE_DRIVERS_ABSTRACT_HPP

#include "cocaine/common.hpp"

namespace cocaine { namespace engine { namespace drivers {

class abstract_driver_t:
    public boost::noncopyable
{
    public:
        virtual ~abstract_driver_t() { }

        inline std::string id() const {
            return m_id;
        }

        inline std::string name() const {
            return m_name;
        }

    protected:
        abstract_driver_t(const std::string& name):
            m_name(name)
        {}
        
    protected:
        std::string m_id;
        const std::string m_name;
};

}}}

#endif
