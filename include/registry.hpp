#include <string>
#include <map>

#include "source.hpp"
#include "mysql.hpp"
#include "loadavg.hpp"

namespace {
    void* loadavg_create(const char* uri) {
        return new loadavg_t(uri);
    }

    void* mysql_create(const char* uri) {
        return new mysql_t(uri);
    }
}

typedef void*(*factory_t)(const char*);

class registry_t {
    public:
        registry_t() {
            m_factories["mysql"] = &mysql_create;
            m_factories["loadavg"] = &loadavg_create;

            syslog(LOG_INFO, "registered mysql handler");
            syslog(LOG_INFO, "registered loadavg handler");
        }

        source_t* create(const std::string& scheme, const std::string& uri) {
            factories_t::iterator it = m_factories.find(scheme);

            if(it == m_factories.end()) {
                throw std::domain_error(scheme);
            }

            factory_t factory = it->second;
            return reinterpret_cast<source_t*>(factory(uri.c_str()));
        }

    private:
        typedef std::map<std::string, factory_t> factories_t;
        factories_t m_factories;
};

