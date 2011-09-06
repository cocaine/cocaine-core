#ifndef YAPPI_FUTURE_HPP
#define YAPPI_FUTURE_HPP

#include "common.hpp"
#include "core.hpp"

namespace yappi { namespace core {

class core_t;

class future_t:
    public boost::noncopyable,
    public helpers::birth_control_t<future_t>
{
    public:
        future_t(core_t *const core, const std::vector<std::string>& route):
            m_core(core),
            m_route(route),
            m_fulfilled(0),
            m_expecting(1)
        {
            syslog(LOG_DEBUG, "promise %s: created", m_id.get().c_str());
        }

    public:
        inline std::string id() const { return m_id.get(); }
        inline std::vector<std::string> route() const { return m_route; }

    public:
        inline void set(const std::string& key, const std::string& value) {
            m_options[key] = value;
        }

        inline std::string get(const std::string& key) {
            option_map_t::iterator it = m_options.find(key);

            if(it != m_options.end()) {
                return it->second;
            } else {
                return "";
            }
        }

        // Push a new slice into this future
        template<class T>
        inline void fulfill(const std::string& key, const T& value) {
            ++m_fulfilled;

            syslog(LOG_DEBUG, "promise %s: slice %u/%u fulfilled", 
                    m_id.get().c_str(), m_fulfilled, m_expecting);
                    
            m_root[key] = value;

            if(m_fulfilled == m_expecting) {
                commit();
            }
        }

        inline Json::Value serialize() {
            Json::Value result;

            result["id"] = m_id.get();
            
            for(option_map_t::const_iterator it = m_options.begin(); it != m_options.end(); ++it) {
                result[it->first] = it->second;
            }

            return result;
        }

        // Set the expected slice count
        inline void await(unsigned int expectation) {
            m_expecting = expectation;
        }

        // Seal the future and return the response
        inline const Json::Value& root() {
            return m_root;
        }

        inline void commit() {
            m_core->seal(m_id.get());
        }

    private:
        // Future ID
        helpers::auto_uuid_t m_id;

        // Parent
        core_t *const m_core;

        // Client identity
        std::vector<std::string> m_route;

        // Slice expectations
        unsigned int m_fulfilled, m_expecting;

        // Resulting document
        Json::Value m_root;

        // Optional arguments
        typedef std::map<std::string, std::string> option_map_t;
        option_map_t m_options;
};

}}

#endif
