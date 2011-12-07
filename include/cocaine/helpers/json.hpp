//
// Copyright (C) 2011 Andrey Sibiryov <me@kobology.ru>
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
