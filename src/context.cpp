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

#include "cocaine/api/logger.hpp"

#include <cerrno>
#include <cstring>
#include <system_error>

#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

#include <netdb.h>

using namespace cocaine;

namespace fs = boost::filesystem;

const float defaults::heartbeat_timeout     = 30.0f;
const float defaults::idle_timeout          = 600.0f;
const float defaults::startup_timeout       = 10.0f;
const float defaults::termination_timeout   = 5.0f;
const unsigned long defaults::pool_limit    = 10L;
const unsigned long defaults::queue_limit   = 100L;
const unsigned long defaults::concurrency   = 10L;

const float defaults::control_timeout       = 5.0f;

const char defaults::plugins_path[]         = "/usr/lib/cocaine";
const char defaults::runtime_path[]         = "/var/run/cocaine";
const char defaults::spool_path[]           = "/var/spool/cocaine";

// Config

namespace {
    void
    validate_path(const fs::path& path) {
        if(!fs::exists(path)) {
            try {
                fs::create_directories(path);
            } catch(const fs::filesystem_error& e) {
                throw configuration_error_t("unable to create the %s path", path);
            }
        }

        if(!fs::is_directory(path)) {
            throw configuration_error_t("the %s path is not a directory", path);
        }
    }
}

config_t::config_t() {
    path.config  = "";
    path.plugins = defaults::plugins_path;
    path.runtime = defaults::runtime_path;
    path.spool   = defaults::spool_path;
}

config_t::config_t(const std::string& config_path) {
    path.config = config_path;

    if(!fs::exists(path.config) || !fs::is_regular(path.config)) {
        throw configuration_error_t("the configuration file path is invalid");
    }

    fs::ifstream stream(path.config);

    if(!stream) {
        throw configuration_error_t("unable to read the configuration file");
    }

    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;

    if(!reader.parse(stream, root)) {
        throw configuration_error_t(
            "the configuration file is corrupted - %s",
            reader.getFormattedErrorMessages()
        );
    }

    // Validation

    if(root.get("version", 0).asUInt() != 2) {
        throw configuration_error_t("the configuration file version is invalid");
    }

    // Paths

    path.plugins = root["paths"].get("plugins", defaults::plugins_path).asString();
    path.runtime = root["paths"].get("runtime", defaults::runtime_path).asString();
    path.spool   = root["paths"].get("spool",   defaults::spool_path  ).asString();

    fs::path runtime = path.runtime;

    validate_path(runtime);
    validate_path(runtime / "engines");
    validate_path(runtime / "services");

    validate_path(path.spool);

    // I/O configuration

    char hostname[256];

    if(gethostname(hostname, 256) != 0) {
        throw std::system_error(
            errno,
            std::system_category(),
            "unable to determine the hostname"
        );
    }

    addrinfo hints,
             * result = nullptr;

    std::memset(&hints, 0, sizeof(addrinfo));

    hints.ai_flags = AI_CANONNAME;

    int rv = getaddrinfo(hostname, nullptr, &hints, &result);

    if(rv != 0) {
        throw configuration_error_t(
            "unable to determine the hostname - %s",
            gai_strerror(rv)
        );
    }

    if(result == nullptr) {
        throw configuration_error_t("unable to determine the hostname");
    }

    network.hostname = result->ai_canonname;

    freeaddrinfo(result);

    // Component configuration

    loggers  = parse(root["loggers"]);
    services = parse(root["services"]);
    storages = parse(root["storages"]);
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

// Context

context_t::context_t(config_t config_,
                     const std::string& logger):
    config(config_)
{
    initialize();

    // Get the default logger for this context.
    m_logger = api::logger(*this, logger);
}

context_t::context_t(config_t config_,
                     std::unique_ptr<logging::logger_t>&& logger):
    config(config_)
{
    initialize();

    // NOTE: The context takes the ownership of the passed logger, so it will
    // become invalid at the calling site after this call.
    m_logger = std::move(logger);
}

context_t::~context_t() {
    // Empty.
}

void
context_t::initialize() {
    m_repository.reset(new api::repository_t());
    m_repository->load(config.path.plugins);
}
