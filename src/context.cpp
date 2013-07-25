/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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
#include "cocaine/api/service.hpp"

#include "cocaine/asio/reactor.hpp"

#include "cocaine/detail/actor.hpp"
#include "cocaine/detail/essentials.hpp"
#include "cocaine/detail/locator.hpp"
#include "cocaine/detail/unique_id.hpp"

#include "cocaine/memory.hpp"

#include <cstring>
#include <system_error>

#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

#include <netdb.h>

using namespace cocaine;

namespace fs = boost::filesystem;

const bool defaults::log_output              = false;
const float defaults::heartbeat_timeout      = 30.0f;
const float defaults::idle_timeout           = 600.0f;
const float defaults::startup_timeout        = 10.0f;
const float defaults::termination_timeout    = 5.0f;
const unsigned long defaults::concurrency    = 10L;
const unsigned long defaults::crashlog_limit = 50L;
const unsigned long defaults::pool_limit     = 10L;
const unsigned long defaults::queue_limit    = 100L;

const float defaults::control_timeout        = 5.0f;
const unsigned defaults::decoder_granularity = 256;

const char defaults::plugins_path[]          = "/usr/lib/cocaine";
const char defaults::runtime_path[]          = "/var/run/cocaine";
const char defaults::spool_path[]            = "/var/spool/cocaine";

const char defaults::endpoint[]              = "::";
const uint16_t defaults::locator_port        = 10053;
const uint16_t defaults::min_port            = 32768;
const uint16_t defaults::max_port            = 61000;

// Config

namespace {

void
validate_path(const fs::path& path) {
    const auto status = fs::status(path);

    if(!fs::exists(status)) {
        throw cocaine::error_t("the %s directory does not exist", path);
    } else if(!fs::is_directory(status)) {
        throw cocaine::error_t("the %s path is not a directory", path);
    }
}

class gai_category_t:
    public std::error_category
{
    virtual
    const char*
    name() const throw() {
        return "getaddrinfo";
    }

    virtual
    std::string
    message(int code) const {
        return gai_strerror(code);
    }
};

gai_category_t category_instance;

const std::error_category&
gai_category() {
    return category_instance;
}

}

config_t::config_t(const std::string& config_path) {
    path.config = config_path;

    const auto status = fs::status(path.config);

    if(!fs::exists(status) || !fs::is_regular_file(status)) {
        throw cocaine::error_t("the configuration file path is invalid");
    }

    fs::ifstream stream(path.config);

    if(!stream) {
        throw cocaine::error_t("unable to read the configuration file");
    }

    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;

    if(!reader.parse(stream, root)) {
        throw cocaine::error_t(
            "the configuration file is corrupted - %s",
            reader.getFormattedErrorMessages()
        );
    }

    // Validation

    if(root.get("version", 0).asUInt() != 2) {
        throw cocaine::error_t("the configuration file version is invalid");
    }

    // Paths

    path.plugins = root["paths"].get("plugins", defaults::plugins_path).asString();
    path.runtime = root["paths"].get("runtime", defaults::runtime_path).asString();
    path.spool   = root["paths"].get("spool",   defaults::spool_path  ).asString();

    validate_path(path.runtime);
    validate_path(path.spool);

    // Hostname configuration

    char hostname[256];

    if(gethostname(hostname, 256) != 0) {
        throw std::system_error(errno, std::system_category(), "unable to determine the hostname");
    }

    addrinfo hints,
             *result = nullptr;

    std::memset(&hints, 0, sizeof(addrinfo));

    hints.ai_flags = AI_CANONNAME;

    const int rv = getaddrinfo(hostname, nullptr, &hints, &result);

    if(rv != 0) {
        throw std::system_error(rv, gai_category(), "unable to determine the hostname");
    }

    network.hostname = result->ai_canonname;
    network.uuid     = unique_id_t().string();

    freeaddrinfo(result);

    // Locator configuration

    network.endpoint = root["locator"].get("endpoint", defaults::endpoint).asString();
    network.locator = root["locator"].get("port", defaults::locator_port).asUInt();

    if(!root["locator"]["port-range"].empty()) {
        network.ports = std::make_tuple(
            root["locator"]["port-range"].get(0u, defaults::min_port).asUInt(),
            root["locator"]["port-range"].get(1u, defaults::max_port).asUInt()
        );
    }

    // Cluster configuration

    if(!root["network"].empty()) {
        if(!root["network"]["group"].empty()) {
            network.group = root["network"]["group"].asString();
        }

        if(!root["network"]["gateway"].empty()) {
            network.gateway = {
                root["network"]["gateway"].get("type", "adhoc").asString(),
                root["network"]["gateway"]["args"]
            };
        }
    }

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

    const Json::Value::Members names(config.getMemberNames());

    for(auto it = names.begin(); it != names.end(); ++it) {
        components[*it] = {
            config[*it].get("type", "unspecified").asString(),
            config[*it]["args"]
        };
    }

    return components;
}

// Context

context_t::context_t(config_t config_, const std::string& logger):
    config(config_)
{
    m_repository.reset(new api::repository_t());

    // Load the builtins.
    essentials::initialize(*m_repository);

    // Load the plugins.
    m_repository->load(config.path.plugins);

    const auto it = config.loggers.find(logger);

    if(it == config.loggers.end()) {
        throw cocaine::error_t("the '%s' logger is not configured", logger);
    }

    // Try to initialize the logger. If this fails, there's no way to report the failure,
    // unfortunately, except printing it to the standart output.
    m_logger = get<api::logger_t>(it->second.type, config, it->second.args);

    bootstrap();
}

context_t::context_t(config_t config_, std::unique_ptr<logging::logger_concept_t>&& logger):
    config(config_)
{
    m_repository.reset(new api::repository_t());

    // Load the builtins.
    essentials::initialize(*m_repository);

    // Load the plugins.
    m_repository->load(config.path.plugins);

    // NOTE: The context takes the ownership of the passed logger, so it will
    // become invalid at the calling site after this call.
    m_logger = std::move(logger);

    bootstrap();
}

context_t::~context_t() {
    auto blog = std::make_unique<logging::log_t>(*this, "bootstrap");

    COCAINE_LOG_INFO(blog, "stopping the service locator");

    m_locator->terminate();

    if(config.network.group) {
        static_cast<locator_t&>(m_locator->dispatch()).disconnect();
    }

    COCAINE_LOG_INFO(blog, "stopping the services");
    
    for(auto it = config.services.rbegin(); it != config.services.rend(); ++it) {
        detach(it->first);
    }
}

void
context_t::attach(const std::string& name, std::unique_ptr<actor_t>&& service) {
    static_cast<locator_t&>(m_locator->dispatch()).attach(name, std::move(service));
}

auto
context_t::detach(const std::string& name) -> std::unique_ptr<actor_t> {
    return static_cast<locator_t&>(m_locator->dispatch()).detach(name);
}

void
context_t::bootstrap() {
    auto blog = std::make_unique<logging::log_t>(*this, "bootstrap");

    // Service locator internals.
    auto reactor = std::make_shared<io::reactor_t>();
    auto locator = std::make_unique<locator_t>(*this, *reactor);

    m_locator.reset(new actor_t(
        reactor,
        std::move(locator)
    ));

    COCAINE_LOG_INFO(
        blog,
        "starting %d %s",
        config.services.size(),
        config.services.size() == 1 ? "service" : "services"
    );

    for(auto it = config.services.begin(); it != config.services.end(); ++it) {
        reactor = std::make_shared<io::reactor_t>();

        COCAINE_LOG_INFO(blog, "starting service '%s'", it->first);

        try {
            attach(it->first, std::make_unique<actor_t>(
                reactor,
                get<api::service_t>(
                    it->second.type,
                    *this,
                    *reactor,
                    cocaine::format("service/%s", it->first),
                    it->second.args
                )
            ));
        } catch(const std::exception& e) {
            COCAINE_LOG_ERROR(blog, "unable to initialize service '%s' - %s", it->first, e.what());
            throw;
        } catch(...) {
            COCAINE_LOG_ERROR(blog, "unable to initialize service '%s' - unknown exception", it->first);
            throw;
        }
    }

    const std::vector<io::tcp::endpoint> endpoints = {
        { boost::asio::ip::address::from_string(config.network.endpoint), config.network.locator }
    };

    COCAINE_LOG_INFO(blog, "starting the service locator on port %d", config.network.locator);

    try {
        if(config.network.group) {
            static_cast<locator_t&>(m_locator->dispatch()).connect();
        }

        // NOTE: Start the locator thread last, so that we won't needlessly send node updates to
        // the peers which managed to connect during the bootstrap.
        m_locator->run(endpoints);
    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(blog, "unable to initialize the locator - %s - [%d] %s", e.what(), e.code().value(), e.code().message());
        throw;
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(blog, "unable to initialize the locator - %s", e.what());
        throw;
    } catch(...) {
        COCAINE_LOG_ERROR(blog, "unable to initialize the locator - unknown exception");
        throw;
    }
}
