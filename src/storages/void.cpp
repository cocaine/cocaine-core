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

#include "cocaine/storages/void.hpp"

using namespace cocaine::storages;

void_storage_t::void_storage_t(context_t& context):
    storage_t(context)
{ }

void void_storage_t::put(const std::string& ns, const std::string& key, const Json::Value& value)
{ }

bool void_storage_t::exists(const std::string& ns, const std::string& key) {
    return false;
}

Json::Value void_storage_t::get(const std::string& ns, const std::string& key) {
    return Json::Value(Json::objectValue);
}

Json::Value void_storage_t::all(const std::string& ns) {
    return Json::Value(Json::objectValue);  
}

void void_storage_t::remove(const std::string& ns, const std::string& key)
{ }

void void_storage_t::purge(const std::string& ns)
{ }
