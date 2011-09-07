#ifndef YAPPI_MONGO_STORAGE_HPP
#define YAPPI_MONGO_STORAGE_HPP

#include <boost/thread/tss.hpp>

#include <mongo/client/dbclient.h>

#include "common.hpp"

namespace yappi { namespace storage { namespace backends {

class mongo_storage_t:
    public boost::noncopyable,
    public helpers::factory_t<mongo_storage_t, boost::thread_specific_ptr>
{
    public:
        mongo_storage_t();

    public:
        void put(const std::string& store, const std::string& key, const Json::Value& value);
        bool exists(const std::string& store, const std::string& key);

        Json::Value get(const std::string& store, const std::string& key);
        Json::Value all(const std::string& store);

        void remove(const std::string& store, const std::string& key);
        void purge(const std::string& store);

    private:
        inline std::string ns(const std::string& store) {
            return m_db + "." + store;
        }

    private:
        std::string m_url, m_db;
        std::auto_ptr<mongo::DBClientConnection> m_connection;
};

}}}

#endif
