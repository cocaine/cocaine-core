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

#include <boost/assign.hpp>

#include "elliptics.hpp"

using namespace cocaine;
using namespace cocaine::storages;

elliptics_storage_t::elliptics_storage_t(context_t& context, const std::string& uri) try:
    category_type(context, uri),
    m_logfile("/tmp/ell-plugin.log", 31),
    m_node(m_logfile)
{
    m_node.add_remote("elisto20f.dev.yandex.net", 1025);
    
    std::vector<int> groups = boost::assign::list_of
        (1)
        (2);
    
    m_node.add_groups(groups);
} catch(const std::runtime_error& e) {
    throw storage_error_t(e.what());
}

void elliptics_storage_t::put(const std::string& ns,
                              const std::string& key,
                              const value_type& value)
{
    try {
        m_node.write_data_wait(
            id(ns, key),
            std::string(
                static_cast<const char*>(value.data()),
                value.size()
            ),
            0,
            0,
            0,
            0
        );
    } catch(const std::runtime_error& e) {
        throw storage_error_t(e.what());
    }
}

bool elliptics_storage_t::exists(const std::string& ns,
                                 const std::string& key)
{
    try {
        return !m_node.lookup(id(ns, key)).empty();
    } catch(const std::runtime_error& e) {
        return false;
    }
}

elliptics_storage_t::value_type elliptics_storage_t::get(const std::string& ns,
                                                         const std::string& key)
{
    std::string value;

    try {
        value = m_node.read_data_wait(
            id(ns, key),
            0,
            0,
            0,
            0,
            0
        );
    } catch(const std::runtime_error& e) {
        throw storage_error_t(e.what());
    }

    return value_type(
        value.data(),
        value.size()
    );
}

std::vector<std::string> elliptics_storage_t::list(const std::string& ns) {
    throw storage_error_t("list() method is not implemented yet");
}

void elliptics_storage_t::remove(const std::string& ns,
                                 const std::string& key)
{
    try {
        m_node.remove(id(ns, key));
    } catch(const std::runtime_error& e) {
        throw storage_error_t(e.what());
    }
}

void elliptics_storage_t::purge(const std::string& ns) {
    throw storage_error_t("purge() method is not implemented yet");
}

extern "C" {
    void initialize(repository_t& repository) {
        repository.insert<elliptics_storage_t>("elliptics");
    }
}
