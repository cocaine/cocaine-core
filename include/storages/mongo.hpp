#ifndef YAPPI_MONGO_STORAGE_HPP
#define YAPPI_MONGO_STORAGE_HPP

#include <mongo/client/dbclient.h>

#include "common.hpp"
#include "storages/abstract.hpp"

namespace yappi { namespace storage { namespace backends {

class mongo_storage_t:
    public abstract_storage_t
{
    public:
        mongo_storage_t();

    public:
        virtual void put(const std::string& store, const std::string& key, const Json::Value& value);
        virtual bool exists(const std::string& store, const std::string& key);

        virtual Json::Value get(const std::string& store, const std::string& key);
        virtual Json::Value all(const std::string& store);

        virtual void remove(const std::string& store, const std::string& key);
        virtual void purge(const std::string& store);

    private:
        inline std::string ns(const std::string& store) {
            return "yappi." + m_instance + "." + store;
        }

    private:
        std::string m_instance;
        mongo::ConnectionString m_url;
};

}}}

#endif
