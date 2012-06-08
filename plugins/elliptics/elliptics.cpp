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

#include <msgpack.hpp>

#include "elliptics.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

using namespace cocaine;
using namespace cocaine::storages;

log_adapter_t::log_adapter_t(context_t& context, const uint32_t mask):
    zbr::elliptics_log(mask),
    m_context(context),
    m_mask(mask),
    m_log(m_context.log("storage/elliptics"))
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

objects::value_type elliptics_storage_t::get(const std::string& ns,
                                             const std::string& key)
{
    objects::value_type object;

    // Fetch the metadata first.
    object.meta = exists(ns, key);
    
    std::string blob;

    try {
        blob = m_node.read_data_wait(
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


    object.blob = objects::data_type(
        blob.data(),
        blob.size()
    );

    return object;
}

void elliptics_storage_t::put(const std::string& ns,
                              const std::string& key,
                              const objects::value_type& object)
{
    try {
        m_node.write_data_wait(
            "meta:" + id(ns, key),
            Json::FastWriter().write(object.meta),
            0,
            0,
            0,
            0
        );
        
        std::string blob;

        blob.assign(
            static_cast<const char*>(object.blob.data()),
            object.blob.size()
        );

        m_node.write_data_wait(
            id(ns, key),
            blob,
            0,
            0,
            0,
            0
        );

        std::vector<std::string> keylist(list(ns));
        
        if(std::find(keylist.begin(), keylist.end(), key) != keylist.end()) {
            return;
        }
        
        msgpack::sbuffer buffer;
        
        keylist.push_back(key);
        msgpack::pack(&buffer, keylist);
        blob.assign(buffer.data(), buffer.size());

        m_node.write_data_wait(
            "list:" + ns,
            blob,
            0,
            0,
            0,
            0
        );
    } catch(const std::runtime_error& e) {
        throw storage_error_t(e.what());
    }
}

objects::meta_type elliptics_storage_t::exists(const std::string& ns,
                                               const std::string& key)
{
    std::string meta;

    try {
        meta = m_node.read_data_wait(
            "meta:" + id(ns, key),
            0,
            0,
            0,
            0,
            0
        );
    } catch(const std::runtime_error& e) {
        throw storage_error_t(e.what());
    }

    Json::Reader reader;
    objects::meta_type object;

    if(!reader.parse(meta, object)) {
        throw storage_error_t("the specified object is corrupted");
    }

    return object;
}

std::vector<std::string> elliptics_storage_t::list(const std::string& ns) {
    std::vector<std::string> result;
    std::string blob;
    
    try {
        blob = m_node.read_data_wait(
            "list:" + ns,
            0,
            0,
            0,
            0,
            0
        );
    } catch(const std::runtime_error& e) {
        return result;
    }

    msgpack::unpacked unpacked;

    msgpack::unpack(&unpacked, blob.data(), blob.size());
    
    try {
        unpacked.get().convert(&result);
    } catch(const std::string& e) {
        throw storage_error_t("the namespace list object is corrupted");
    }

    return result;
}

void elliptics_storage_t::remove(const std::string& ns,
                                 const std::string& key)
{
    try {
        m_node.remove("meta:" + id(ns, key));
        m_node.remove(id(ns, key));

        std::vector<std::string> keylist(list(ns)),
                                 updated;

        std::remove_copy(
            keylist.begin(),
            keylist.end(),
            std::back_inserter(updated),
            key
        );

        msgpack::sbuffer buffer;
        std::string blob;

        msgpack::pack(&buffer, updated);
        blob.assign(buffer.data(), buffer.size());

        m_node.write_data_wait(
            "list:" + ns,
            blob,
            0,
            0,
            0,
            0
        );
    } catch(const std::runtime_error& e) {
        throw storage_error_t(e.what());
    }
}

extern "C" {
    void initialize(repository_t& repository) {
        repository.insert<elliptics_storage_t>("elliptics");
    }
}
