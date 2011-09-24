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
        abstract_driver_t(threading::overseer_t* parent, boost::shared_ptr<plugin::source_t> source):
            m_parent(parent),
            m_source(source) 
        {}
        
        void publish(const Json::Value& result) {
            if(m_pipe.get() && !m_id.empty() && !result.isNull()) {
                m_pipe->send_multi(boost::make_tuple(m_id, result));
            }
        }

    protected:
        // Driver ID
        std::string m_id;
        
        // Parent
        threading::overseer_t* m_parent;
        
        // Data source
        boost::shared_ptr<plugin::source_t> m_source;
        
        // Messaging
        std::auto_ptr<lines::channel_t> m_pipe;
};

}}}

#endif
