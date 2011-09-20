#ifndef COCAINE_FUTURE_HPP
#define COCAINE_FUTURE_HPP

#include <set>

#include "cocaine/common.hpp"
#include "cocaine/core.hpp"

namespace cocaine { namespace core {

class core_t;

class future_t:
    public boost::noncopyable,
    public helpers::birth_control_t<future_t>
{
    public:
        future_t(core_t* core, const std::vector<std::string>& route):
            m_core(core),
            m_route(route)
        {
            syslog(LOG_DEBUG, "future %s: created", m_id.get().c_str());
        }

    public:
        inline std::string id() const { 
            return m_id.get();
        }
        
        inline std::vector<std::string> route() const { 
            return m_route;
        }

        template<class T>
        inline void fulfill(const std::string& key, const T& value) {
            std::set<std::string>::iterator it = m_reserve.find(key);
            
            if(it != m_reserve.end()) {
                m_root[key] = value;
                m_reserve.erase(it);

                if(m_reserve.empty()) {
                    m_core->seal(m_id.get());
                }
            } else {
                syslog(LOG_ERR, "future %s: invalid key - %s",
                    m_id.get().c_str(), key.c_str());
            }
        }

        inline void abort(const std::string& message) {
            m_root.clear();
            m_root["error"] = message;
            m_core->seal(m_id.get());
        }

        inline const Json::Value& root() {
            return m_root;
        }

    private:
        friend class core_t;
        
        template<class T>
        inline void reserve(const T& reserve) {
            m_reserve.insert(reserve.begin(), reserve.end());
        }


    private:
        // Future ID
        helpers::auto_uuid_t m_id;

        // Parent
        core_t* m_core;

        // Client identity
        std::vector<std::string> m_route;

        // Reserved keys
        std::set<std::string> m_reserve;
        
        // Resulting document
        Json::Value m_root;
};

}}

#endif
