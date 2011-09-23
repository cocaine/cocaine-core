#include <mongo/client/connpool.h>

#include "cocaine/storage/mongo.hpp"

using namespace cocaine::storage::backends;
using namespace mongo;

mongo_storage_t::mongo_storage_t() try:
    m_instance(config_t::get().core.instance),
    m_url(config_t::get().storage.location, ConnectionString::SET)
{
    if(!m_url.isValid()) {
        throw std::runtime_error("invalid mongodb url");
    }
} catch(const DBException& e) {
    throw std::runtime_error(e.what());
}

void mongo_storage_t::put(const std::string& store, const std::string& key, const Json::Value& value) {
    Json::FastWriter writer;
    Json::Value nested;

    nested["key"] = key;
    nested["value"] = value;

    std::string json(writer.write(nested));

    // NOTE: For some reason, fromjson fails to parse strings with double null-terminator
    // which is exactly the kind of strings JSONCPP generates, hence the chopping.
    BSONObj object(fromjson(json.substr(0, json.length() - 1)));
    
    try {
        ScopedDbConnection connection(m_url);
        connection->ensureIndex(ns(store), BSON("key" << 1), true); // Unique index
        connection->update(ns(store), BSON("key" << key), object, true); // Upsert
        connection.done();
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }
}

bool mongo_storage_t::exists(const std::string& store, const std::string& key) {
    bool result;
    
    try {
        ScopedDbConnection connection(m_url);
        result = connection->count(ns(store), BSON("key" << key));
        connection.done();
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }

    return result;
}

Json::Value mongo_storage_t::get(const std::string& store, const std::string& key) {
    Json::Reader reader;
    Json::Value result;
    BSONObj object;

    try {
        ScopedDbConnection connection(m_url);
        object = connection->findOne(ns(store), BSON("key" << key));
        connection.done();
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }

    if(!object.isEmpty()) {
        if(reader.parse(object.jsonString(), result)) {
            return result["value"];
        } else {
            throw std::runtime_error("corrupted data in '" + store + "'");
        }
    }

    return result;
}

Json::Value mongo_storage_t::all(const std::string& store) {
    Json::Reader reader;
    Json::Value root, result;

    try {
        ScopedDbConnection connection(m_url);
        std::auto_ptr<DBClientCursor> cursor(connection->query(ns(store), BSONObj()));
        
        while(cursor->more()) {
            if(reader.parse(cursor->nextSafe().jsonString(), result)) {
                root[result["key"].asString()] = result["value"];
            } else {
                throw std::runtime_error("corrupted data in '" + store + "'");
            }
        }
        
        connection.done();
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }

    return root;
}

void mongo_storage_t::remove(const std::string& store, const std::string& key) {
    try {
        ScopedDbConnection connection(m_url);
        connection->remove(ns(store), BSON("key" << key));
        connection.done();
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }
}

void mongo_storage_t::purge(const std::string& store) {
    try {
        ScopedDbConnection connection(m_url);
        connection->remove(ns(store), BSONObj());
        connection.done();
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }
}

