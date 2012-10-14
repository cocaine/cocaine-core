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
#include <boost/format.hpp>
#include <boost/tuple/tuple.hpp>
#include <netdb.h>

#include "cocaine/context.hpp"

#include "cocaine/io.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/isolates/process.hpp"
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
const long defaults::control_timeout = 500L;
const unsigned long defaults::io_bulk_size = 100L;

const char defaults::ipc_path[] = "/var/run/cocaine";
const char defaults::plugin_path[] = "/usr/lib/cocaine";
const char defaults::spool_path[] = "/var/spool/cocaine";

namespace {
    void
    validate_path(const fs::path& path) {
        if(!fs::exists(path)) {
            boost::format message("the '%s' path does not exist");
            throw configuration_error_t((message % path.string()).str());
        } else if(fs::exists(path) && !fs::is_directory(path)) {
            boost::format message("the '%s' path is not a directory");
            throw configuration_error_t((message % path.string()).str());
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

    if(root.get("version", 0).asUInt() != 2) {
        throw configuration_error_t("the configuration version is invalid");
    }

    ipc_path = root["paths"].get("ipc", defaults::ipc_path).asString();
    validate_path(ipc_path);

    plugin_path = root["paths"].get("plugins", defaults::plugin_path).asString();
    validate_path(plugin_path);

    spool_path = root["paths"].get("spool", defaults::spool_path).asString();
    validate_path(spool_path);

    // Component configuration

    components = parse(root["components"]);

    // IO configuration

    char hostname[256];

    if(gethostname(hostname, 256) == 0) {
        addrinfo hints,
                 * result;
        
        memset(&hints, 0, sizeof(addrinfo));
        hints.ai_flags = AI_CANONNAME;

        int rv = getaddrinfo(hostname, NULL, &hints, &result);
        
        if(rv != 0) {
            boost::format message("unable to determine the hostname - %s");
            throw configuration_error_t((message % gai_strerror(rv)).str());
        }

        if(result == NULL) {
            throw configuration_error_t("unable to determine the hostname");
        }
        
        runtime.hostname = result->ai_canonname;
        freeaddrinfo(result);
    } else {
        throw system_error_t("unable to determine the hostname");
    }
}

config_t::component_map_t
config_t::parse(const Json::Value& config) {
    component_map_t components;

    if(config.empty()) {
        return components;
    }

    Json::Value::Members names(config.getMemberNames());

    for(Json::Value::Members::const_iterator it = names.begin();
        it != names.end();
        ++it)
    {
        component_t info = {
            config[*it].get("type", "not specified").asString(),
            config[*it]["args"]
        };

        components.emplace(*it, info);
    }

    return components;
}

context_t::context_t(config_t config_, boost::shared_ptr<logging::sink_t> sink):
    config(config_),
    m_sink(sink)
{
    if(!m_sink) {
        m_sink.reset(new logging::void_sink_t());
    }

    m_repository.reset(new api::repository_t(*this));
    m_repository->load(config.plugin_path);

    // Register the builtin components.
    m_repository->insert<isolate::process_t>("process");
    m_repository->insert<storage::file_storage_t>("files");
    
    m_io.reset(new zmq::context_t(1));
}

context_t::~context_t() {
    BOOST_ASSERT(io::socket_t::objects_alive() == 0);
}

boost::shared_ptr<logging::logger_t>
context_t::log(const std::string& name) {
    boost::lock_guard<boost::mutex> lock(m_mutex);

    instance_map_t::iterator it(m_instances.find(name));

    if(it == m_instances.end()) {
        boost::tie(it, boost::tuples::ignore) = m_instances.emplace(
            name,
            boost::make_shared<logging::logger_t>(
                *m_sink,
                name
            )
        );
    }

    return it->second;
}
