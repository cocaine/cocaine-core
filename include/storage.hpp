#ifndef YAPPI_STORAGE_HPP
#define YAPPI_STORAGE_HPP

#include "file_storage.hpp"

namespace yappi { namespace storage {

typedef backends::file_storage_t storage_t;

class proxy_t {
    public:
        proxy_t(storage_t* storage, const std::string& prefix):
            m_storage(storage),
            m_prefix(prefix)
        {}

        bool set(const std::string& key, const Json::Value& value) {
            Json::Value object = m_storage->get(m_prefix);
            object["store"][key] = value;
            m_storage->put(m_prefix, object);
        }
        
        Json::Value get(const std::string& key) {
            Json::Value object = m_storage->get(m_prefix);
            return object["store"][key];
        }
        
    private:
        storage_t* m_storage;
        std::string m_prefix;
};

}}

#endif
