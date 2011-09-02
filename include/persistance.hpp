#ifndef YAPPI_PERSISTANCE_HPP
#define YAPPI_PERSISTANCE_HPP

#include "common.hpp"
#include "file_storage.hpp"
// #include "eblob_storage.hpp"

namespace yappi { namespace persistance {

template<class Backend>
class storage_facade_t:
    public Backend,
    public helpers::factory_t< storage_facade_t<Backend> >
{
    public:
        storage_facade_t(const config_t& config):
            Backend(config),
            m_config(config)
        {
            if(m_config.storage.disabled) {
                syslog(LOG_DEBUG, "storage: running in a transient mode");
            }
        }

    public:
        bool put(const std::string& key, const Json::Value& value) {
            if(m_config.storage.disabled)
                return false;

            return static_cast<Backend*>(this)->put(key, value);
        }

        bool exists(const std::string& key) const {
            if(m_config.storage.disabled)
                return false;

            return static_cast<const Backend*>(this)->exists(key);
        }

        Json::Value get(const std::string& key) const {
            if(m_config.storage.disabled)
                return Json::Value();

            return static_cast<const Backend*>(this)->get(key);
        }

        Json::Value all() const {
            if(m_config.storage.disabled)
                return Json::Value();

            return static_cast<const Backend*>(this)->all();
        }

        void remove(const std::string& key) {
            if(m_config.storage.disabled)
                return;

            static_cast<Backend*>(this)->remove(key);
        }

        void purge() {
            if(m_config.storage.disabled)
                return;

            static_cast<Backend*>(this)->purge();
        }

    private:
        const config_t& m_config;
};

typedef storage_facade_t<backends::file_storage_t> storage_t;

}}

#endif
