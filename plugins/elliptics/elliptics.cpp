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

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::storages;

log_adapter_t::log_adapter_t(context_t& context, const uint32_t mask):
    zbr::elliptics_log(mask),
    m_context(context),
    m_mask(mask),
    m_log(m_context.log("elliptics"))
{ }

void log_adapter_t::log(const uint32_t mask, const char * message) {
    size_t length = ::strlen(message) - 1;
    
    switch(mask) {
        case DNET_LOG_NOTICE:
            m_log->info("%.*s", length, message);
            break;

        case DNET_LOG_INFO:
            m_log->info("%.*s", length, message);
            break;

        case DNET_LOG_TRANS:
            m_log->debug("%.*s", length, message);
            break;

        case DNET_LOG_ERROR:
            m_log->error("%.*s", length, message);
            break;

        case DNET_LOG_DSA:
            m_log->debug("%.*s", length, message);
            break;

        default:
            break;
    };
}

unsigned long log_adapter_t::clone() {
    return reinterpret_cast<unsigned long>(
        new log_adapter_t(m_context, m_mask)
    );
}

namespace {
    struct digitizer {
        template<class T>
        int operator()(const T& value) {
            return value.asInt();
        }
    };
}

elliptics_storage_t::elliptics_storage_t(context_t& context, const Json::Value& args):
    category_type(context, args),
    m_log_adapter(context, args.get("verbosity", DNET_LOG_ERROR).asUInt()),
    m_node(m_log_adapter)
{
    Json::Value nodes(args["nodes"]);

    if(nodes.empty() || !nodes.isObject()) {
        throw storage_error_t("no nodes has been specified");
    }

    Json::Value::Members node_names(nodes.getMemberNames());

    for(Json::Value::Members::const_iterator it = node_names.begin();
        it != node_names.end();
        ++it)
    {
        try {
            m_node.add_remote(
                it->c_str(),
                nodes[*it].asInt()
            );
        } catch(const std::runtime_error& e) {
            // Do nothing. Yes. Really.
        }
    }

    Json::Value groups(args["groups"]);

    if(groups.empty() || !groups.isArray()) {
        throw storage_error_t("no groups has been specified");
    }

    std::vector<int> group_numbers;

    std::transform(
        groups.begin(),
        groups.end(),
        std::back_inserter(group_numbers),
        digitizer()
    );

    m_node.add_groups(group_numbers);
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
