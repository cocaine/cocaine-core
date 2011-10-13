#ifndef COCAINE_DRIVERS_ABSTRACT_HPP
#define COCAINE_DRIVERS_ABSTRACT_HPP

#include "cocaine/common.hpp"
#include "cocaine/engine.hpp"

namespace cocaine { namespace engine { namespace drivers {

class abstract_driver_t:
    public boost::noncopyable
{
    public:
        virtual ~abstract_driver_t() { };

        inline std::string id() const {
            return m_id;
        }

    protected:
        abstract_driver_t(const std::string& name, boost::shared_ptr<engine_t> parent):
            m_name(name),
            m_parent(parent)
        {}
        
    protected:
        std::string m_id;
        std::string m_name;

        boost::shared_ptr<engine_t> m_parent;
};

}}}

#endif
