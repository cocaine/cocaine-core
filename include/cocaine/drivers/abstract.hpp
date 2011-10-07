#ifndef COCAINE_DRIVERS_ABSTRACT_HPP
#define COCAINE_DRIVERS_ABSTRACT_HPP

#include "cocaine/common.hpp"
#include "cocaine/plugin.hpp"
#include "cocaine/threading.hpp"

namespace cocaine { namespace engine { namespace drivers {

using namespace cocaine::security;

class abstract_driver_t:
    public boost::noncopyable
{
    public:
        virtual ~abstract_driver_t() {};

        inline std::string id() const {
            return m_id;
        }

    protected:
        abstract_driver_t(boost::shared_ptr<overseer_t> parent):
            m_parent(parent)
        {}
        
        void publish(const Json::Value& result) {
            if(!m_id.empty() && !result.isNull()) {
                m_parent->link().send_multi(boost::make_tuple(
                    EVENT,
                    m_id,
                    result));
            }
        }

    protected:
        // Driver ID
        std::string m_id;
        
        // Parent
        boost::shared_ptr<overseer_t> m_parent;
};

}}}

#endif
