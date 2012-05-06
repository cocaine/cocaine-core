//
// Copyright (C) 2011-2012 Andrey Sibiryov <me@kobology.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <mongo/client/connpool.h>

#include "mongo.hpp"

#include "cocaine/context.hpp"

#include "cocaine/registry.hpp"

using namespace cocaine::core;
using namespace cocaine::storages;
using namespace mongo;

mongo_storage_t::mongo_storage_t(context_t& ctx) try:
    storage_t(ctx),
    m_uri(ctx.config.storage.uri, ConnectionString::SET)
{
    if(!m_uri.isValid()) {
        throw storage_error_t("invalid mongodb uri");
    }
} catch(const DBException& e) {
    throw storage_error_t(e.what());
}

void mongo_storage_t::put(const std::string& ns,
                          const std::string& key,
                          const Json::Value& value)
{
    Json::FastWriter writer;
    Json::Value container(Json::objectValue);

    container["key"] = key;
    container["object"] = value;

    // NOTE: Stupid double-conversion magic. 
    // For some reason, fromjson fails to parse strings with double null-terminator
    // which is exactly the kind of strings JSONCPP generates, hence the chopping.
    std::string json(writer.write(container));
    BSONObj object(fromjson(json.substr(0, json.size() - 1)));
    
    try {
        ScopedDbConnection connection(m_uri);
        connection->ensureIndex(resolve(ns), BSON("key" << 1), true); // Unique index
        connection->update(resolve(ns), BSON("key" << key), object, true); // Upsert
        connection.done();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }
}

bool mongo_storage_t::exists(const std::string& ns, const std::string& key) {
    bool result;
    
    try {
        ScopedDbConnection connection(m_uri);
        result = connection->count(resolve(ns), BSON("key" << key));
        connection.done();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }

    return result;
}

Json::Value mongo_storage_t::get(const std::string& ns, const std::string& key) {
    Json::Reader reader;
    Json::Value result(Json::objectValue);
    BSONObj object;

    try {
        ScopedDbConnection connection(m_uri);
        object = connection->findOne(resolve(ns), BSON("key" << key));
        connection.done();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }

    if(!object.isEmpty()) {
        if(reader.parse(object.jsonString(), result)) {
            return result["object"];
        } else {
            throw storage_error_t("corrupted data in '" + ns + "'");
        }
    }

    return result;
}

Json::Value mongo_storage_t::all(const std::string& ns) {
    Json::Reader reader;
    Json::Value root(Json::objectValue), result;

    try {
        ScopedDbConnection connection(m_uri);
        std::auto_ptr<DBClientCursor> cursor(connection->query(resolve(ns), BSONObj()));
        
        while(cursor->more()) {
            if(reader.parse(cursor->nextSafe().jsonString(), result)) {
                root[result["key"].asString()] = result["object"];
            } else {
                throw storage_error_t("corrupted data in '" + ns + "'");
            }
        }
        
        connection.done();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }

    return root;
}

void mongo_storage_t::remove(const std::string& ns, const std::string& key) {
    try {
        ScopedDbConnection connection(m_uri);
        connection->remove(resolve(ns), BSON("key" << key));
        connection.done();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }
}

void mongo_storage_t::purge(const std::string& ns) {
    try {
        ScopedDbConnection connection(m_uri);
        connection->remove(resolve(ns), BSONObj());
        connection.done();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }
}

std::string mongo_storage_t::resolve(const std::string& ns) const {
    return "cocaine." + ns;
}

extern "C" {
    void initialize(registry_t& registry) {
        registry.insert<mongo_storage_t, storage_t>("mongodb");
    }
}
