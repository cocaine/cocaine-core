#ifndef COCAINE_MONGO_STORAGE_HPP
#define COCAINE_MONGO_STORAGE_HPP

#include <mongo/client/dbclient.h>

#include "cocaine/storages/abstract.hpp"

namespace cocaine { namespace storage { namespace backends {

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
            return "cocaine." + m_instance + "." + store;
        }

    private:
        const std::string m_instance;
        const mongo::ConnectionString m_url;
};

}}}

#endif
