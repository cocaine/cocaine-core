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
#include "cocaine/asio/resolver.hpp"

#include "cocaine/detail/actor.hpp"
#include "cocaine/detail/essentials.hpp"
#include "cocaine/detail/locator.hpp"
#include "cocaine/detail/unique_id.hpp"

#include "cocaine/memory.hpp"

#include "cocaine/traits/graph.hpp"
#include "cocaine/traits/tuple.hpp"

#include <cstring>

#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

#include <netdb.h>

using namespace cocaine;
using namespace cocaine::io;

using namespace std::placeholders;

namespace fs = boost::filesystem;

#include "report.inl"
#include "synchronization.inl"

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

const char defaults::endpoint[]              = "::";
const uint16_t defaults::locator_port        = 10053;
const uint16_t defaults::min_port            = 32768;
const uint16_t defaults::max_port            = 61000;

// Config

config_t::config_t(const std::string& config_path) {
    path.config = config_path;

    const auto config_file_status = fs::status(path.config);

    if(!fs::exists(config_file_status) || !fs::is_regular_file(config_file_status)) {
        throw cocaine::error_t("the configuration file path is invalid");
    }

    fs::ifstream stream(path.config);

    if(!stream) {
        throw cocaine::error_t("unable to read the configuration file");
    }

    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;

    if(!reader.parse(stream, root)) {
        throw cocaine::error_t("the configuration file is corrupted - %s", reader.getFormattedErrorMessages());
    }

    // Validation

    if(root.get("version", 0).asUInt() != 2) {
        throw cocaine::error_t("the configuration file version is invalid");
    }

    // Paths

    path.plugins = root["paths"].get("plugins", defaults::plugins_path).asString();
    path.runtime = root["paths"].get("runtime", defaults::runtime_path).asString();

    const auto runtime_path_status = fs::status(path.runtime);

    if(!fs::exists(runtime_path_status)) {
        throw cocaine::error_t("the %s directory does not exist", path.runtime);
    } else if(!fs::is_directory(runtime_path_status)) {
        throw cocaine::error_t("the %s path is not a directory", path.runtime);
    }

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

    network.hostname = root["locator"].get("hostname", result->ai_canonname).asString();
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

int
config_t::version() {
    return COCAINE_VERSION;
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

    // Try to initialize the logger. If this fails, there's no way to report the failure, unfortunately,
    // except printing it to the standart output.
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

    // NOTE: The context takes the ownership of the passed logger, so it will become invalid at the
    // calling site after this call.
    m_logger = std::move(logger);

    bootstrap();
}

context_t::~context_t() {
    auto blog = std::make_unique<logging::log_t>(*this, "bootstrap");

    m_synchronization->shutdown();
    m_synchronization.reset();

    COCAINE_LOG_INFO(blog, "stopping the service locator");

    m_services.front().second->terminate();
    m_services.pop_front();

    COCAINE_LOG_INFO(blog, "stopping the services");
    
    for(auto it = config.services.rbegin(); it != config.services.rend(); ++it) {
        detach(it->first);
    }

    // Any services which haven't been explicitly detached by their owners must be terminated here,
    // because otherwise their threads' destructors will terminate the whole program (13.3.1.3).

    while(!m_services.empty()) {
        m_services.back().second->terminate();
        m_services.pop_back();
    }
}

namespace {

struct match {
    template<class T>
    bool
    operator()(const T& service) const {
        return name == service.first;
    }

    const std::string& name;
};

}

void
context_t::attach(const std::string& name, std::unique_ptr<actor_t>&& service) {
    uint16_t port = 0;

    std::lock_guard<std::mutex> guard(m_mutex);

    auto existing = std::find_if(m_services.cbegin(), m_services.cend(), match {
        name
    });

    BOOST_VERIFY(existing == m_services.end());

    if(config.network.ports) {
        if(m_ports.empty()) {
            throw cocaine::error_t("no ports left for allocation");
        }

        port = m_ports.top();

        // NOTE: Remove the taken port from the free pool. If, for any reason, this port is unavailable
        // for binding, it's okay to keep it removed forever.
        m_ports.pop();
    }

    const std::vector<io::tcp::endpoint> endpoints = {{
        boost::asio::ip::address::from_string(config.network.endpoint),
        port
    }};

    service->run(endpoints);

    auto blog = std::make_unique<logging::log_t>(*this, "bootstrap");

    COCAINE_LOG_INFO(blog, "service '%s' published on %d", name, service->location().front());

    m_services.emplace_back(name, std::move(service));

    if(m_synchronization) {
        m_synchronization->announce();
    }
}

auto
context_t::detach(const std::string& name) -> std::unique_ptr<actor_t> {
    std::lock_guard<std::mutex> guard(m_mutex);

    auto it = std::find_if(m_services.begin(), m_services.end(), match {
        name
    });

    BOOST_VERIFY(it != m_services.end());

    // Release the service's actor ownership.
    std::unique_ptr<actor_t> service = std::move(it->second);
    m_services.erase(it);

    const std::vector<io::tcp::endpoint> endpoints = service->location();

    service->terminate();

    auto blog = std::make_unique<logging::log_t>(*this, "bootstrap");

    COCAINE_LOG_INFO(blog, "service '%s' withdrawn from %d", name, endpoints.front());

    if(config.network.ports) {
        m_ports.push(endpoints.front().port());
    }

    if(m_synchronization) {
        m_synchronization->announce();
    }

    return service;
}

auto
context_t::locate(const std::string& name) const -> boost::optional<actor_t&> {
    std::lock_guard<std::mutex> guard(m_mutex);

    auto it = std::find_if(m_services.begin(), m_services.end(), match {
        name
    });

    return boost::optional<actor_t&>(it != m_services.end(), *it->second);
}

void
context_t::bootstrap() {
    auto blog = std::make_unique<logging::log_t>(*this, "bootstrap");

    if(config.network.ports) {
        uint16_t min, max;

        std::tie(min, max) = config.network.ports.get();

        COCAINE_LOG_INFO(blog, "%u ports available, %u through %u", max - min, min, max);

        while(min != max) {
            m_ports.push(--max);
        }
    }

    COCAINE_LOG_INFO(
        blog,
        "starting %d %s",
        config.services.size(),
        config.services.size() == 1 ? "service" : "services"
    );

    for(auto it = config.services.begin(); it != config.services.end(); ++it) {
        auto reactor = std::make_shared<reactor_t>();

        COCAINE_LOG_INFO(blog, "starting service '%s'", it->first);

        try {
            attach(it->first, std::make_unique<actor_t>(*this, reactor, get<api::service_t>(
                it->second.type,
                *this,
                *reactor,
                cocaine::format("service/%s", it->first),
                it->second.args
            )));
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(blog, "unable to initialize service '%s' - %s - [%d] %s", it->first, e.what(),
                e.code().value(), e.code().message());
            throw;
        } catch(const std::exception& e) {
            COCAINE_LOG_ERROR(blog, "unable to initialize service '%s' - %s", it->first, e.what());
            throw;
        } catch(...) {
            COCAINE_LOG_ERROR(blog, "unable to initialize service '%s' - unknown exception", it->first);
            throw;
        }
    }

    // NOTE: Start the locator thread last, so that we won't needlessly send node updates to
    // the peers which managed to connect during the bootstrap.

    const std::vector<io::tcp::endpoint> endpoints = {{
        boost::asio::ip::address::from_string(config.network.endpoint),
        config.network.locator
    }};

    COCAINE_LOG_INFO(blog, "starting the service locator on %s", endpoints.front());

    auto reactor = std::make_shared<reactor_t>();
    auto locator = std::make_unique<locator_t>(*this, *reactor);

    m_synchronization = std::make_shared<synchronization_t>(*this);

    // NOTE: Some of the locator methods are better implemented in the Context, to avoid unnecessary
    // copying intermediate structures around.

    locator->on<io::locator::synchronize>(m_synchronization);
    locator->on<io::locator::reports>(memusage_action_t { *this });

    m_services.emplace_front("locator", std::make_unique<actor_t>(
        *this,
        reactor,
        std::unique_ptr<dispatch_t>(std::move(locator))
    ));

    try {
        m_services.front().second->run(endpoints);
    } catch(const std::system_error& e) {
        COCAINE_LOG_ERROR(blog, "unable to initialize the locator - %s - [%d] %s", e.what(),
            e.code().value(), e.code().message());
        throw;
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(blog, "unable to initialize the locator - %s", e.what());
        throw;
    } catch(...) {
        COCAINE_LOG_ERROR(blog, "unable to initialize the locator - unknown exception");
        throw;
    }
}
