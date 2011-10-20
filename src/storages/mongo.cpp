#include <mongo/client/connpool.h>

#include "cocaine/storages/mongo.hpp"

using namespace cocaine::storage::backends;
using namespace mongo;

mongo_storage_t::mongo_storage_t() try:
    m_instance(config_t::get().core.instance),
    m_url(config_t::get().storage.location, ConnectionString::SET)
{
    if(!m_url.isValid()) {
        throw std::runtime_error("invalid mongodb uri");
    }
} catch(const DBException& e) {
    throw std::runtime_error(e.what());
}

void mongo_storage_t::put(const std::string& ns, const std::string& key, const Json::Value& value) {
    Json::FastWriter writer;
    Json::Value container(Json::objectValue);

    container["key"] = key;
    container["object"] = value;

    std::string json(writer.write(container));

    // NOTE: For some reason, fromjson fails to parse strings with double null-terminator
    // which is exactly the kind of strings JSONCPP generates, hence the chopping.
    BSONObj object(fromjson(json.substr(0, json.length() - 1)));
    
    try {
        ScopedDbConnection connection(m_url);
        connection->ensureIndex(resolve(ns), BSON("key" << 1), true); // Unique index
        connection->update(resolve(ns), BSON("key" << key), object, true); // Upsert
        connection.done();
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }
}

bool mongo_storage_t::exists(const std::string& ns, const std::string& key) {
    bool result;
    
    try {
        ScopedDbConnection connection(m_url);
        result = connection->count(resolve(ns), BSON("key" << key));
        connection.done();
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }

    return result;
}

Json::Value mongo_storage_t::get(const std::string& ns, const std::string& key) {
    Json::Reader reader;
    Json::Value result(Json::objectValue);
    BSONObj object;

    try {
        ScopedDbConnection connection(m_url);
        object = connection->findOne(resolve(ns), BSON("key" << key));
        connection.done();
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }

    if(!object.isEmpty()) {
        if(reader.parse(object.jsonString(), result)) {
            return result["object"];
        } else {
            throw std::runtime_error("corrupted data in '" + ns + "'");
        }
    }

    return result;
}

Json::Value mongo_storage_t::all(const std::string& ns) {
    Json::Reader reader;
    Json::Value root(Json::objectValue), result;

    try {
        ScopedDbConnection connection(m_url);
        std::auto_ptr<DBClientCursor> cursor(connection->query(resolve(ns), BSONObj()));
        
        while(cursor->more()) {
            if(reader.parse(cursor->nextSafe().jsonString(), result)) {
                root[result["key"].asString()] = result["object"];
            } else {
                throw std::runtime_error("corrupted data in '" + ns + "'");
            }
        }
        
        connection.done();
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }

    return root;
}

void mongo_storage_t::remove(const std::string& ns, const std::string& key) {
    try {
        ScopedDbConnection connection(m_url);
        connection->remove(resolve(ns), BSON("key" << key));
        connection.done();
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }
}

void mongo_storage_t::purge(const std::string& ns) {
    try {
        ScopedDbConnection connection(m_url);
        connection->remove(resolve(ns), BSONObj());
        connection.done();
    } catch(const DBException& e) {
        throw std::runtime_error(e.what());
    }
}

