#include "detail/mongo.hpp"

using namespace yappi::helpers;
using namespace yappi::storage::backends;
using namespace mongo;

mongo_storage_t::mongo_storage_t():
    m_url(config_t::get().storage.path),
    m_db(config_t::get().core.instance)
{
    m_connection.reset(new mongo::DBClientConnection(true));

    try {
        m_connection->connect(m_url);
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }
}

void mongo_storage_t::put(const std::string& store, const std::string& key, const Json::Value& value) {
    Json::FastWriter writer;
    Json::Value nested;

    nested["key"] = key;
    nested["value"] = value;

    std::string json = writer.write(nested);
    BSONObj object = fromjson(json.substr(0, json.length() - 1));
    
    try {
        m_connection->update(ns(store), BSON("key" << key),
            object, true);
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }
}

bool mongo_storage_t::exists(const std::string& store, const std::string& key) {
    try {
        return m_connection->count(ns(store), BSON("key" << key));
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }
}

Json::Value mongo_storage_t::get(const std::string& store, const std::string& key) {
    Json::Reader reader;
    Json::Value result;
    BSONObj object;

    try {
        object = m_connection->findOne(ns(store), BSON("key" << key));
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }

    if(!object.isEmpty()) {
        if(reader.parse(object.jsonString(), result)) {
            return result["value"];
        } else {
            throw std::runtime_error("data corruption in '" + store + "'");
        }
    }

    return result;
}

Json::Value mongo_storage_t::all(const std::string& store) {
    Json::Reader reader;
    Json::Value root, result;
    std::auto_ptr<DBClientCursor> cursor;

    try {
        cursor = m_connection->query(ns(store), BSONObj());
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }

    while(cursor->more()) {
        try {
            if(reader.parse(cursor->nextSafe().jsonString(), result)) {
                root[result["key"].asString()] = result["value"];
            } else {
                throw std::runtime_error("data corruption in '" + store + "'");
            }
        } catch(const AssertionException& e) {
            throw std::runtime_error(e.what());
        }
    }
    
    return root;
}

void mongo_storage_t::remove(const std::string& store, const std::string& key) {
    try {
        m_connection->remove(ns(store), BSON("key" << key));
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }
}

void mongo_storage_t::purge(const std::string& store) {
    syslog(LOG_NOTICE, "storage: purging '%s'", store.c_str());
    
    try {
        m_connection->remove(ns(store), BSONObj());
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }
}

