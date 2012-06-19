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

#include <boost/filesystem/fstream.hpp>

#include "cocaine/context.hpp"

#include "cocaine/io.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/storages/files.hpp"

using namespace cocaine;
using namespace cocaine::storages;

namespace fs = boost::filesystem;

const float defaults::heartbeat_timeout = 30.0f;
const float defaults::suicide_timeout = 600.0f;
const float defaults::termination_timeout = 5.0f;
const unsigned int defaults::pool_limit = 10;
const unsigned int defaults::queue_limit = 100;
const unsigned int defaults::io_bulk_size = 100;
const char defaults::slave[] = "cocaine-slave";
const char defaults::plugin_path[] = "/usr/lib/cocaine";
const char defaults::ipc_path[] = "/var/run/cocaine";
const char defaults::spool_path[] = "/var/spool/cocaine";

namespace {
    void validate_path(const fs::path& path) {
        if(!fs::exists(path)) {
            throw configuration_error_t("the specified path '" + path.string() + "' does not exist");
        } else if(fs::exists(path) && !fs::is_directory(path)) {
            throw configuration_error_t("the specified path '" + path.string() + "' is not a directory");
        }
    }
}

config_t::config_t(const std::string& path):
    config_path(path)
{
    if(!fs::exists(config_path)) {
        throw configuration_error_t("the configuration file doesn't exist");
    }

    fs::ifstream stream(config_path);

    if(!stream) {
        throw configuration_error_t("unable to open the configuration file");
    }

    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;

    if(!reader.parse(stream, root)) {
        throw configuration_error_t("the configuration file is corrupted");
    }

    // Validation
    // ----------

    if(root.get("version", 0).asUInt() != 1) {
        throw configuration_error_t("the configuration version is invalid");
    }

    // Paths
    // -----

    plugin_path = root["paths"].get("plugins", defaults::plugin_path).asString();
    
    validate_path(plugin_path);

    spool_path = root["paths"].get("spool", defaults::spool_path).asString();
    
    validate_path(spool_path);

    ipc_path = root["paths"].get("ipc", defaults::ipc_path).asString();
    
    validate_path(ipc_path);

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
        plugin_config_t config = {
            *it,
            root["storages"][*it]["args"]
        };

        storage_info_t info = {
            root["storages"][*it]["type"].asString(),
            config
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

    // IO configuration
    // ----------------

    char hostname[HOSTNAME_MAX_LENGTH];

    if(gethostname(hostname, HOSTNAME_MAX_LENGTH) == 0) {
        runtime.hostname = hostname;
    } else {
        throw system_error_t("failed to determine the hostname");
    }
}

context_t::context_t(config_t config_, boost::shared_ptr<logging::sink_t> sink):
    config(config_),
    m_sink(sink)
{
    if(!m_sink) {
        m_sink.reset(new logging::void_sink_t());
    }

    // Initialize the component repository.
    m_repository.reset(new repository_t(*this));

    // Register the builtin components.
    m_repository->insert<file_storage_t>("files");
    
    // Initialize the ZeroMQ context.
    m_io.reset(new zmq::context_t(1));
}

context_t::~context_t() {
    BOOST_ASSERT(io::socket_t::objects_alive == 0);
}

boost::shared_ptr<logging::logger_t>
context_t::log(const std::string& name) {
    return m_sink->get(name);
}
