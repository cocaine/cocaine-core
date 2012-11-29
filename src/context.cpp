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

#include "cocaine/context.hpp"

#include "cocaine/io.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/isolates/process.hpp"

#include "cocaine/loggers/files.hpp"
#include "cocaine/loggers/stdout.hpp"
#include "cocaine/loggers/syslog.hpp"

#include "cocaine/storages/files.hpp"

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/tuple/tuple.hpp>

#include <netdb.h>

using namespace cocaine;

namespace fs = boost::filesystem;

const char defaults::slave[] = "/usr/bin/cocaine-worker-generic";

const float defaults::heartbeat_timeout = 30.0f;
const float defaults::idle_timeout = 600.0f;
const float defaults::startup_timeout = 10.0f;
const float defaults::termination_timeout = 5.0f;
const unsigned long defaults::pool_limit = 10L;
const unsigned long defaults::queue_limit = 100L;
const unsigned long defaults::concurrency = 10L;

const long defaults::control_timeout = 500L;
const unsigned long defaults::io_bulk_size = 100L;

const char defaults::ipc_path[] = "/var/run/cocaine";
const char defaults::plugin_path[] = "/usr/lib/cocaine";
const char defaults::spool_path[] = "/var/spool/cocaine";

namespace {
    void
    validate_path(const fs::path& path) {
        if(!fs::exists(path)) {
            throw configuration_error_t("the '%s' path does not exist", path.string());
        } else if(fs::exists(path) && !fs::is_directory(path)) {
            throw configuration_error_t("the '%s' path is not a directory", path.string());
        }
    }
}

config_t::config_t(const std::string& path):
    config_path(path)
{
    if(!fs::exists(config_path)) {
        throw configuration_error_t("the configuration path doesn't exist");
    }

    if(!fs::is_regular(config_path)) {
        throw configuration_error_t("the configuration path doesn't point to a file");
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
        
        std::memset(&hints, 0, sizeof(addrinfo));

        hints.ai_flags = AI_CANONNAME;

        int rv = getaddrinfo(hostname, NULL, &hints, &result);
        
        if(rv != 0) {
            throw configuration_error_t("unable to determine the hostname - %s", gai_strerror(rv));
        }

        if(result == NULL) {
            throw configuration_error_t("unable to determine the hostname");
        }
        
        network.hostname = result->ai_canonname;

        freeaddrinfo(result);
    } else {
        throw system_error_t("unable to determine the hostname");
    }

    // Port mapper

    Json::Value range(root["port-mapper"]["range"]);

    network.ports = {
        range[0].asUInt(),
        range[1].asUInt()
    };
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
            config[*it].get("type", "unspecified").asString(),
            config[*it]["args"]
        };

        components.emplace(*it, info);
    }

    return components;
}

port_mapper_t::port_mapper_t(const std::pair<uint16_t, uint16_t>& limits):
    m_ports(
        boost::make_counting_iterator(limits.first),
        boost::make_counting_iterator(limits.second)
    )
{ }

uint16_t
port_mapper_t::get() {
    boost::unique_lock<boost::mutex> lock(m_mutex);

    if(m_ports.empty()) {
        throw cocaine::error_t("no available ports left");
    }

    uint16_t port = m_ports.top();
    m_ports.pop();

    return port;
}

void
port_mapper_t::retain(uint16_t port) {
    boost::unique_lock<boost::mutex> lock(m_mutex);
    m_ports.push(port);
}

context_t::context_t(config_t config_):
    config(config_)
{
    m_repository.reset(new api::repository_t());
    
    // Register the builtin isolates.
    m_repository->insert<isolate::process_t>("process");

    // Register the builtin loggers
    m_repository->insert<logger::files_t>("files");
    m_repository->insert<logger::stdout_t>("stdout");
    m_repository->insert<logger::syslog_t>("syslog");
    
    // Register the builtin storages.
    m_repository->insert<storage::files_t>("files");

    // Register the plugins.
    m_repository->load(config.plugin_path);

    // Initialize the ZeroMQ context.
    m_io.reset(new zmq::context_t(1));

    // Initialize the port mapper.
    m_port_mapper.reset(new port_mapper_t(config.network.ports));
}

context_t::~context_t() {
    // NOTE: Plugin categories have to be destroyed in a specific order,
    // so that loggers would be destroyed after all the shared factories
    // which may use logging subsystem. For now, it involes storages only.
    m_repository->dispose<api::storage_t>();
}

boost::shared_ptr<logging::logger_t>
context_t::log(const std::string& name) {
    boost::lock_guard<boost::mutex> lock(m_mutex);

    if(!m_sink) {
        config_t::component_t cfg;

        try {
            cfg = config.components.at("logger/default");
        } catch(const std::out_of_range&) {
            cfg.type = "stdout";
        }

        // Get the logging sink.
        m_sink = get<api::logger_t>(
            cfg.type,
            "cocaine",
            cfg.args
        );
    }

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
