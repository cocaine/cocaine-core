/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>. 
*/

#include <mongo/client/connpool.h>
#include <mongo/client/gridfs.h>

#include "mongo.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::storages;
using namespace mongo;

mongo_storage_t::mongo_storage_t(context_t& context, const plugin_config_t& config) try:
    category_type(context, config),
    m_log(context.log("storage/" + config.name)),
    m_uri(config.args["uri"].asString(), ConnectionString::SET)
{
    if(!m_uri.isValid()) {
        throw storage_error_t("invalid mongodb uri");
    }
} catch(const DBException& e) {
    throw storage_error_t(e.what());
}

objects::value_type mongo_storage_t::get(const std::string& ns,
                                         const std::string& key)
{
    objects::value_type result;

    try {
        // Fetch the metadata.
        result.meta = exists(ns, key);
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }

    try {
        ScopedDbConnection connection(m_uri);
        GridFS gridfs(connection.conn(), "cocaine", ns);
        GridFile file(gridfs.findFile(key));

        // Fetch the blob.
        if(!file.exists()) {
            throw storage_error_t("the specified object has not been found");
        }

        connection.done();

        std::cout << file.getMetadata() << std::endl;

        std::stringstream buffer;
        std::string blob;

        file.write(buffer);
        blob = buffer.str();

        result.blob = objects::data_type(
            blob.data(),
            blob.size()
        );
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }

    return result;
}

void mongo_storage_t::put(const std::string& ns,
                          const std::string& key,
                          const objects::value_type& value)
{
    Json::FastWriter writer;
    Json::Value container(Json::objectValue);

    container["key"] = key;
    container["object"] = value.meta;

    // NOTE: Stupid double-conversion magic. 
    // For some reason, fromjson fails to parse strings with double null-terminator
    // which is exactly the kind of strings JSONCPP generates, hence the chopping.
    std::string json(writer.write(container));
    BSONObj object(fromjson(json.substr(0, json.size() - 1)));
    
    try {
        ScopedDbConnection connection(m_uri);
        
        // Store the metadata.
        connection->ensureIndex(resolve(ns), BSON("key" << 1), true); // Unique index
        connection->update(resolve(ns), BSON("key" << key), object, true); // Upsert
        
        GridFS gridfs(connection.conn(), "cocaine", ns);

        // Store the blob.
        BSONObj result = gridfs.storeFile(
            static_cast<const char*>(value.blob.data()),
            value.blob.size(),
            key
        );

        connection.done();

        std::cout << result << std::endl;
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }
}

objects::meta_type mongo_storage_t::exists(const std::string& ns,
                                           const std::string& key)
{
    BSONObj object;

    try {
        // Fetch the metadata.
        ScopedDbConnection connection(m_uri);
        object = connection->findOne(resolve(ns), BSON("key" << key));
        connection.done();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }

    if(!object.isEmpty()) {
        objects::meta_type meta;
        Json::Reader reader;

        if(reader.parse(object.jsonString(), meta)) {
            return meta["object"];
        } else {
            throw storage_error_t("the specified object is corrupted");
        }
    } else {
        throw storage_error_t("the specified object has not been found");
    }
}

std::vector<std::string> mongo_storage_t::list(const std::string& ns) {
    std::vector<std::string> result;
    BSONObj object;

    try {
        ScopedDbConnection connection(m_uri);
        std::auto_ptr<DBClientCursor> cursor(connection->query(resolve(ns), BSONObj()));
        
        while(cursor->more()) {
            object = cursor->nextSafe();
            result.push_back(object["key"]);
        }
        
        connection.done();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }

    return result;
}

void mongo_storage_t::remove(const std::string& ns,
                             const std::string& key)
{
    try {
        ScopedDbConnection connection(m_uri);

        // Remove the metadata.
        connection->remove(resolve(ns), BSON("key" << key));

        GridFS gridfs(connection.conn(), "cocaine", ns);

        // Remove the blob.
        gridfs.removeFile(key);

        connection.done();
    } catch(const DBException& e) {
        throw storage_error_t(e.what());
    }
}

extern "C" {
    void initialize(repository_t& repository) {
        repository.insert<mongo_storage_t>("mongodb");
    }
}
