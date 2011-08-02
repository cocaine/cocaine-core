#ifndef YAPPI_FUTURE_HPP
#define YAPPI_FUTURE_HPP

#include "common.hpp"
#include "core.hpp"

namespace yappi { namespace core {

class core_t;

class future_t: boost::noncopyable {
    public:
        // Initialize the client's future with it's 0MQ identity
        future_t(core_t* core, const std::vector<std::string>& identity):
            m_core(core),
            m_identity(identity),
            m_fulfilled(0),
            m_expecting(1)
        {
            syslog(LOG_DEBUG, "future %s: created", m_id.get().c_str());
        }

        // Initialize internal future
        future_t(core_t* core):
            m_core(core),
            m_fulfilled(0),
            m_expecting(1)
        {
            syslog(LOG_DEBUG, "future %s: created", m_id.get().c_str());
        }

    public:
        inline std::string id() const { return m_id.get(); }
        inline std::string token() const { return m_token; }
        inline std::vector<std::string> identity() const { return m_identity; }

    public:
        inline void assign(const std::string& token) {
            m_token = token;
        }

        // Push a new slice into this future
        template<class T>
        void fulfill(const std::string& key, const T& value) {
            ++m_fulfilled;

            syslog(LOG_DEBUG, "future %s: slice %u/%u fulfilled", 
                    m_id.get().c_str(), m_fulfilled, m_expecting);
                    
            m_root[key] = value;

            if(m_fulfilled == m_expecting) {
                m_core->seal(m_id.get());
            }
        }

        Json::Value serialize() {
            Json::Value result;

            result["id"] = m_id.get();
            result["token"] = m_token;

            return result;
        }

        // Set the expected slice count
        inline void await(unsigned int expectation) {
            m_expecting = expectation;
        }

        // Seal the future and return the response
        std::string seal();

    private:
        // Future ID
        helpers::auto_uuid_t m_id;

        // Parent
        core_t* m_core;

        // Client identity
        std::string m_token;
        std::vector<std::string> m_identity;

        // Slice expectations
        unsigned int m_fulfilled, m_expecting;

        // Resulting document
        Json::Value m_root;
};

}}

#endif
