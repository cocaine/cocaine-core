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
#include <boost/filesystem/operations.hpp>
#include <boost/tuple/tuple.hpp>

#include <netdb.h>

#include "cocaine/context.hpp"

#include "cocaine/io.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/storages/files.hpp"

using namespace cocaine;

namespace fs = boost::filesystem;

const char defaults::slave[] = "/usr/bin/cocaine-slave";
const float defaults::heartbeat_timeout = 30.0f;
const float defaults::idle_timeout = 600.0f;
const float defaults::startup_timeout = 10.0f;
const float defaults::termination_timeout = 5.0f;
const unsigned long defaults::pool_limit = 10L;
const unsigned long defaults::queue_limit = 100L;

const long defaults::bus_timeout = 1000L;
const unsigned long defaults::io_bulk_size = 100L;

const char defaults::ipc_path[] = "/var/run/cocaine";
const char defaults::plugin_path[] = "/usr/lib/cocaine";
const char defaults::spool_path[] = "/var/spool/cocaine";

namespace {
    void
    validate_path(const fs::path& path) {
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

    if(root.get("version", 0).asUInt() != 2) {
        throw configuration_error_t("the configuration version is invalid");
    }

    // Paths
    // -----

    ipc_path = root["paths"].get("ipc", defaults::ipc_path).asString();
    validate_path(ipc_path);

    plugin_path = root["paths"].get("plugins", defaults::plugin_path).asString();
    validate_path(plugin_path);

    spool_path = root["paths"].get("spool", defaults::spool_path).asString();
    validate_path(spool_path);

    // Component configuration
    // -----------------------

    Json::Value::Members component_names(
        root["components"].getMemberNames()
    );

    for(Json::Value::Members::const_iterator it = component_names.begin();
        it != component_names.end();
        ++it)
    {
        component_info_t info = {
            root["components"][*it]["type"].asString(),
            root["components"][*it]["args"]
        };

        components.insert(
            std::make_pair(
                *it,
                info
            )
        );
    }

    // IO configuration
    // ----------------

    char hostname[256];

    if(gethostname(hostname, 256) == 0) {
        addrinfo hints,
                 * result;
        
        memset(&hints, 0, sizeof(addrinfo));
        hints.ai_flags = AI_CANONNAME;

        int rv = getaddrinfo(hostname, NULL, &hints, &result);
        
        if(rv != 0) {
            std::string message(gai_strerror(rv));
            throw configuration_error_t("unable to determine the hostname - " + message);
        }

        if(result == NULL) {
            throw configuration_error_t("unable to determine the hostname - no hostnames have been configured for the host");
        }
        
        runtime.hostname = result->ai_canonname;
        freeaddrinfo(result);
    } else {
        throw system_error_t("unable to determine the hostname");
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
    m_repository.reset(new api::repository_t(*this));
    m_repository->load(config.plugin_path);

    // Register the builtin components.
    m_repository->insert<storage::file_storage_t>("files");
    
    // Initialize the ZeroMQ context.
    m_io.reset(new zmq::context_t(1));
}

context_t::~context_t() {
    BOOST_ASSERT(io::socket_t::objects_alive() == 0);
}

boost::shared_ptr<logging::logger_t>
context_t::log(const std::string& name) {
    boost::lock_guard<boost::mutex> lock(m_mutex);

    instance_map_t::iterator it(
        m_instances.find(name)
    );

    if(it == m_instances.end()) {
        boost::tie(it, boost::tuples::ignore) = m_instances.insert(
            std::make_pair(
                name,
                boost::make_shared<logging::logger_t>(
                    *m_sink,
                    name
                )
            )
        );
    }

    return it->second;
}
