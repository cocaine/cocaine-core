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

#include <boost/filesystem/fstream.hpp>

#include "cocaine/context.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/networking.hpp"

#include "cocaine/storages/void.hpp"
#include "cocaine/storages/files.hpp"

using namespace cocaine;
using namespace cocaine::storages;

namespace fs = boost::filesystem;

const float defaults::heartbeat_timeout = 30.0f;
const float defaults::suicide_timeout = 600.0f;
const unsigned int defaults::pool_limit = 10;
const unsigned int defaults::queue_limit = 100;
const unsigned int defaults::io_bulk_size = 100;

config_t::config_t(const std::string& path) {
    if(!fs::exists(path)) {
        throw configuration_error_t("the specified configuration file doesn't exist");
    }

    fs::ifstream stream(path);

    if(!stream) {
        throw configuration_error_t("unable to open the specified configuration file");
    }

    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;

    if(!reader.parse(stream, root)) {
        throw configuration_error_t("the specified configuration file is invalid");
    }

    // Component repository configuration
    // ----------------------------------

    module_path = root.get("module-path", "").asString();

    // Storage configuration
    // ---------------------

    if(!root["storages"].isObject() || root["storages"].empty()) {
        throw configuration_error_t("no storages has been configured");
    }

    Json::Value::Members storage_names(root["storages"].getMemberNames());

    for(Json::Value::Members::const_iterator it = storage_names.begin();
        it != storage_names.end();
        ++it)
    {
        storage_info_t info = {
            root["storages"][*it]["type"].asString(),
            root["storages"][*it]["uri"].asString()
        };

        storages.insert(
            std::make_pair(
                *it,
                info
            )
        );
    }

    if(storages.find("core") == storages.end()) {
        throw configuration_error_t("mandatory 'core' storage has not been configured");
    }

    // Networking configuration
    // ------------------------

    char hostname[HOSTNAME_MAX_LENGTH];

    if(gethostname(hostname, HOSTNAME_MAX_LENGTH) == 0) {
        runtime.hostname = hostname;
    } else {
        throw system_error_t("failed to determine the hostname");
    }   
}

context_t::context_t(config_t config_):
    config(config_)
{
    // Initialize the component repository.
    m_repository.reset(new repository_t(*this));

    // Register the builtins.
    m_repository->insert<void_storage_t, storage_t>("void");
    m_repository->insert<file_storage_t, storage_t>("files");
}

context_t::~context_t() { }

zmq::context_t& context_t::io() {
    boost::lock_guard<boost::mutex> lock(m_mutex);
    
    if(!m_io.get()) {
        m_io.reset(new zmq::context_t(1));
    }

    return *m_io;
}

category_traits<storage_t>::ptr_type
context_t::storage(const std::string& name) {
    config_t::storage_info_map_t::const_iterator it(
        config.storages.find(name)
    );
    
    if(it == config.storages.end()) {
        throw configuration_error_t("the specified storage doesn't exist");
    }

    return get<storages::storage_t>(
        it->second.type,
        category_traits<storage_t>::args_type(
            it->second.uri
        )
    );
}

boost::shared_ptr<logging::logger_t>
context_t::log(const std::string& name) {
    return sink->get(name);
}
