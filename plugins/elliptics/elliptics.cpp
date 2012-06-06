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

#include "elliptics.hpp"

using namespace cocaine;
using namespace cocaine::storages;

elliptics_storage_t::elliptics_storage_t(context_t& context, const std::string& uri):
    category_type(context, uri)
{
}

void elliptics_storage_t::put(const std::string& ns,
                              const std::string& key,
                              const value_type& value)
{
}

bool elliptics_storage_t::exists(const std::string& ns,
                                 const std::string& key)
{
    return false;
}

elliptics_storage_t::value_type elliptics_storage_t::get(const std::string& ns,
                                                         const std::string& key)
{
    return value_type();
}

std::vector<std::string> elliptics_storage_t::list(const std::string& ns) {
    return std::vector<std::string>();
}

void elliptics_storage_t::remove(const std::string& ns,
                                 const std::string& key)
{
}

void elliptics_storage_t::purge(const std::string& ns) {
}

extern "C" {
    void initialize(repository_t& repository) {
        repository.insert<elliptics_storage_t>("elliptics");
    }
}
