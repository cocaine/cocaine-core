#ifndef COCAINE_STORAGES_MONGO_HPP
#define COCAINE_STORAGES_MONGO_HPP

#include <mongo/client/dbclient.h>

#include "cocaine/storages/abstract.hpp"

namespace cocaine { namespace storage { namespace backends {

class mongo_storage_t:
    public storage_t
{
    public:
        mongo_storage_t();

        virtual void put(const std::string& ns, const std::string& key, const Json::Value& value);

        virtual bool exists(const std::string& ns, const std::string& key);
        virtual Json::Value get(const std::string& ns, const std::string& key);
        virtual Json::Value all(const std::string& ns);

        virtual void remove(const std::string& ns, const std::string& key);
        virtual void purge(const std::string& ns);

    private:
        inline std::string resolve(const std::string& ns) {
            return "cocaine." + m_instance + "." + ns;
        }

    private:
        const std::string m_instance;
        const mongo::ConnectionString m_url;
};

}}}

#endif
