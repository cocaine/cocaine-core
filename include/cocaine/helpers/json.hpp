#ifndef COCAINE_HELPERS_JSON
#define COCAINE_HELPERS_JSON

#include "json/json.h"

namespace cocaine { namespace helpers {

template<class T>
inline Json::Value make_json(const std::string& key, const T& value) {
    Json::Value object(Json::objectValue);
    
    object[key] = value;
    
    return object;
}

}}

#endif
