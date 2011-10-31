#ifndef COCAINE_HELPERS_JSON
#define COCAINE_HELPERS_JSON

#include "json/json.h"

namespace cocaine { namespace helpers {

typedef Json::Value json;

template<class T>
json make_json(const std::string& key, const T& value) {
    json object(Json::objectValue);

    object[key] = value;

    return object;
}

//std::string serialize_json(const json& object) {
//    Json::FastWriter writer;
//    return writer.write(object);
//}

}}

#endif
