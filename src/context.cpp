/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

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
#include "cocaine/defaults.hpp"

#include "cocaine/api/service.hpp"

#include "cocaine/detail/actor.hpp"
#include "cocaine/detail/bootstrap/logging.hpp"
#include "cocaine/detail/engine.hpp"
#include "cocaine/detail/essentials.hpp"

#ifdef COCAINE_ALLOW_RAFT
    #include "cocaine/detail/raft/repository.hpp"
    #include "cocaine/detail/raft/node_service.hpp"
    #include "cocaine/detail/raft/control_service.hpp"
#endif

#include "cocaine/logging.hpp"

#include <numeric>
#include <random>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

#include "rapidjson/reader.h"

#include <blackhole/formatter/json.hpp>
#include <blackhole/frontend/files.hpp>
#include <blackhole/frontend/syslog.hpp>
#include <blackhole/scoped_attributes.hpp>
#include <blackhole/sink/socket.hpp>

using namespace cocaine;

namespace fs = boost::filesystem;

namespace {

struct dynamic_reader_t {
    void
    Null() {
        m_stack.emplace(dynamic_t::null);
    }

    void
    Bool(bool v) {
        m_stack.emplace(v);
    }

    void
    Int(int v) {
        m_stack.emplace(v);
    }

    void
    Uint(unsigned v) {
        m_stack.emplace(dynamic_t::uint_t(v));
    }

    void
    Int64(int64_t v) {
        m_stack.emplace(v);
    }

    void
    Uint64(uint64_t v) {
        m_stack.emplace(dynamic_t::uint_t(v));
    }

    void
    Double(double v) {
        m_stack.emplace(v);
    }

    void
    String(const char* data, size_t size, bool) {
        m_stack.emplace(dynamic_t::string_t(data, size));
    }

    void
    StartObject() {
        // Empty.
    }

    void
    EndObject(size_t size) {
        dynamic_t::object_t object;

        for(size_t i = 0; i < size; ++i) {
            dynamic_t value = std::move(m_stack.top());
            m_stack.pop();

            std::string key = std::move(m_stack.top().as_string());
            m_stack.pop();

            object[key] = std::move(value);
        }

        m_stack.emplace(std::move(object));
    }

    void
    StartArray() {
        // Empty.
    }

    void
    EndArray(size_t size) {
        dynamic_t::array_t array(size);

        for(size_t i = size; i != 0; --i) {
            array[i - 1] = std::move(m_stack.top());
            m_stack.pop();
        }

        m_stack.emplace(std::move(array));
    }

    dynamic_t
    Result() {
        return m_stack.top();
    }

private:
    std::stack<dynamic_t> m_stack;
};

struct rapidjson_ifstream_t {
    rapidjson_ifstream_t(fs::ifstream* backend) :
        m_backend(backend)
    { }

    char
    Peek() const {
        int next = m_backend->peek();

        if(next == std::char_traits<char>::eof()) {
            return '\0';
        } else {
            return next;
        }
    }

    char
    Take() {
        int next = m_backend->get();

        if(next == std::char_traits<char>::eof()) {
            return '\0';
        } else {
            return next;
        }
    }

    size_t
    Tell() const {
        return m_backend->gcount();
    }

    char*
    PutBegin() {
        assert(false);
        return 0;
    }

    void
    Put(char) {
        assert(false);
    }

    size_t
    PutEnd(char*) {
        assert(false);
        return 0;
    }

private:
    fs::ifstream* m_backend;
};

} // namespace

namespace cocaine {

template<>
struct dynamic_converter<config_t::component_t> {
    typedef config_t::component_t result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        return config_t::component_t {
            from.as_object().at("type", "unspecified").as_string(),
            from.as_object().at("args", dynamic_t::object_t())
        };
    }
};

template<>
struct dynamic_converter<config_t::logging_t> {
    typedef config_t::logging_t result_type;

    static
    result_type
    convert(const dynamic_t& from) {
        result_type component;
        const auto& logging = from.as_object();

        for(auto it = logging.begin(); it != logging.end(); ++it) {
            using namespace blackhole::repository;

            const auto object = it->second.as_object();
            const auto loggers = object.at("loggers", dynamic_t::array_t());

            config_t::logging_t::logger_t log {
                logmask(object.at("verbosity", defaults::log_verbosity).as_string()),
                object.at("timestamp", defaults::log_timestamp).as_string(),
                config::parser::adapter_t<dynamic_t, blackhole::log_config_t>::parse(it->first, loggers)
            };

            component.loggers[it->first] = log;
        }

        return component;
    }

    static inline
    logging::priorities
    logmask(const std::string& verbosity) {
        if(verbosity == "debug") {
            return logging::debug;
        } else if(verbosity == "warning") {
            return logging::warning;
        } else if(verbosity == "error") {
            return logging::error;
        } else {
            return logging::info;
        }
    }
};

} // namespace cocaine

config_t::config_t(const std::string& source) {
    const auto source_file_status = fs::status(source);

    if(!fs::exists(source_file_status) || !fs::is_regular_file(source_file_status)) {
        throw cocaine::error_t("configuration file path is invalid");
    }

    fs::ifstream stream(source);

    if(!stream) {
        throw cocaine::error_t("unable to read configuration file");
    }

    rapidjson::MemoryPoolAllocator<> json_allocator;
    rapidjson::Reader json_reader(&json_allocator);
    rapidjson_ifstream_t json_stream(&stream);

    dynamic_reader_t configuration_constructor;

    if(!json_reader.Parse<rapidjson::kParseDefaultFlags>(json_stream, configuration_constructor)) {
        if(json_reader.HasParseError()) {
            throw cocaine::error_t("configuration file is corrupted - %s", json_reader.GetParseError());
        } else {
            throw cocaine::error_t("configuration file is corrupted");
        }
    }

    const auto root = configuration_constructor.Result();

    // Version validation

    if(root.as_object().at("version", 0).to<unsigned int>() != 3) {
        throw cocaine::error_t("configuration file version is invalid");
    }

    const auto path_config    = root.as_object().at("paths",   dynamic_t::object_t()).as_object();
    const auto network_config = root.as_object().at("network", dynamic_t::object_t()).as_object();

    // Path configuration

    path.plugins = path_config.at("plugins", defaults::plugins_path).as_string();
    path.runtime = path_config.at("runtime", defaults::runtime_path).as_string();

    const auto runtime_path_status = fs::status(path.runtime);

    if(!fs::exists(runtime_path_status)) {
        throw cocaine::error_t("directory %s does not exist", path.runtime);
    } else if(!fs::is_directory(runtime_path_status)) {
        throw cocaine::error_t("%s is not a directory", path.runtime);
    }

    // Network configuration

    network.endpoint = boost::asio::ip::address::from_string(
        network_config.at("endpoint", defaults::endpoint).as_string()
    );

    boost::asio::io_service asio;
    boost::asio::ip::tcp::resolver resolver(asio);
    boost::asio::ip::tcp::resolver::iterator it, end;

    try {
        it = resolver.resolve(boost::asio::ip::tcp::resolver::query(
            boost::asio::ip::host_name(), std::string(),
            boost::asio::ip::tcp::resolver::query::canonical_name
        ));
    } catch(const boost::system::system_error& e) {
#if defined(HAVE_GCC48)
        std::throw_with_nested(cocaine::error_t("unable to determine local hostname"));
#else
        throw cocaine::error_t("unable to determine local hostname");
#endif
    }

    network.hostname = network_config.at("hostname", it->host_name()).as_string();
    network.pool     = network_config.at("pool", boost::thread::hardware_concurrency() * 2).as_uint();

    if(network.pool <= 0) {
        throw cocaine::error_t("network I/O pool size must be positive");
    }

    if(network_config.count("pinned")) {
        network.ports.pinned = network_config.at("pinned").to<decltype(network.ports.pinned)>();
    }

    if(network_config.count("shared")) {
        network.ports.shared = network_config.at("shared").to<decltype(network.ports.shared)>();
    }

    // Blackhole logging configuration
    logging = root.as_object().at("logging",  dynamic_t::empty_object).to<config_t::logging_t>();

    // Component configuration
    services = root.as_object().at("services", dynamic_t::empty_object).to<config_t::component_map_t>();
    storages = root.as_object().at("storages", dynamic_t::empty_object).to<config_t::component_map_t>();

#ifdef COCAINE_ALLOW_RAFT
    create_raft_cluster = false;
#endif
}

int
config_t::versions() {
    return COCAINE_VERSION;
}

// Dynamic port mapper

port_mapping_t::port_mapping_t(const config_t& config):
    m_pinned(config.network.ports.pinned)
{
    port_t minimum, maximum;

    std::tie(minimum, maximum) = config.network.ports.shared;

    std::vector<port_t> seed;

    if((minimum == 0 && maximum == 0) || maximum <= minimum) {
        seed.resize(65535);
        std::fill(seed.begin(), seed.end(), 0);
    } else {
        seed.resize(maximum - minimum);
        std::iota(seed.begin(), seed.end(), minimum);
    }

    std::random_device device;
    std::shuffle(seed.begin(), seed.end(), std::default_random_engine(device()));

    // Safe until fully constructed.
    m_shared.unsafe() = queue_type(seed.begin(), seed.end());
}

port_t
port_mapping_t::assign(const std::string& name) {
    if(m_pinned.count(name)) {
        return m_pinned.at(name);
    }

    auto ptr = m_shared.synchronize();

    if(ptr->empty()) {
        throw cocaine::error_t("no ports left for allocation");
    }

    const auto port = ptr->front(); ptr->pop_front();

    return port;
}

void
port_mapping_t::retain(const std::string& name, port_t port) {
    if(m_pinned.count(name)) {
        // TODO: Fix pinned ports retention.
        return;
    }

    return m_shared->push_back(port);
}

// Context

context_t::context_t(config_t config_, const std::string& logger_backend):
    config(config_),
    mapper(config_)
{
    auto& repository = blackhole::repository_t::instance();

    // Available logging sinks.
    typedef boost::mpl::vector<
        blackhole::sink::stream_t,
        blackhole::sink::files_t<>,
        blackhole::sink::syslog_t<logging::priorities>,
        blackhole::sink::socket_t<boost::asio::ip::tcp>,
        blackhole::sink::socket_t<boost::asio::ip::udp>
    > sinks_t;

    // Available logging formatters.
    typedef boost::mpl::vector<
        blackhole::formatter::string_t,
        blackhole::formatter::json_t
    > formatters_t;

    // Register frontends with all combinations of formatters and sinks with the logging repository.
    repository.configure<sinks_t, formatters_t>();

    try {
        using blackhole::keyword::tag::timestamp_t;
        using blackhole::keyword::tag::severity_t;

        // Fetch configuration object.
        auto logger = config.logging.loggers.at(logger_backend);

        // Configure some mappings for timestamps and severity attributes.
        blackhole::mapping::value_t mapper;

        mapper.add<severity_t<logging::priorities>>(&logging::map_severity);
        mapper.add<timestamp_t>(logger.timestamp);

        // Attach them to the logging config.
        auto& frontends = logger.config.frontends;

        for(auto it = frontends.begin(); it != frontends.end(); ++it) {
            it->formatter.mapper = mapper;
        }

        // Register logger configuration with the Blackhole's repository.
        repository.add_config(logger.config);

        typedef logging::logger_t logger_type;

        // Try to initialize the logger. If it fails, there's no way to report the failure, except
        // printing it to the standart output.
        m_logger = std::make_unique<logger_type>(repository.create<logging::priorities>(logger_backend));
        m_logger->verbosity(logger.verbosity);
    } catch(const std::out_of_range&) {
        throw cocaine::error_t("logger '%s' is not configured", logger_backend);
    }

    bootstrap();
}

context_t::context_t(config_t config_, std::unique_ptr<logging::logger_t> logger):
    config(config_),
    mapper(config_)
{
    m_logger = std::move(logger);

    bootstrap();
}

context_t::~context_t() {
    blackhole::scoped_attributes_t guard(
       *m_logger,
        blackhole::log::attributes_t({logging::keyword::source() = "core"})
    );

    COCAINE_LOG_INFO(m_logger, "stopping %d service(s)", m_services->size());

    // Fire off to alert concerned subscribers about the shutdown. This signal happens before all
    // the outstanding connections are closed, so services have a change to send their last wishes.
    signals.shutdown();

    // Stop the service from accepting new clients or doing any processing. Pop them from the active
    // service list into this temporary storage, and then destroy them all at once. This is needed
    // because sessions in the execution units might still have references to the services, and their
    // lives have to be extended until those sessions are active.
    std::vector<std::unique_ptr<actor_t>> actors;

    for(auto it = config.services.rbegin(); it != config.services.rend(); ++it) {
        try {
            actors.push_back(remove(it->first));
        } catch(const cocaine::error_t& e) {
            // A service might be absent because it has failed to start during the bootstrap.
            continue;
        }
    }

    // There should be no outstanding services left. All the extra services spawned by others, like
    // app invocation services from the node service, should be dead by now.
    BOOST_ASSERT(m_services->empty());

    COCAINE_LOG_INFO(m_logger, "stopping %d execution unit(s)", m_pool.size());

    m_pool.clear();

    // Destroy the service objects.
    actors.clear();

    COCAINE_LOG_INFO(m_logger, "core has been terminated");
}

std::unique_ptr<logging::log_t>
context_t::log(const std::string& source, blackhole::log::attributes_t attributes) {
    return std::make_unique<logging::log_t>(*m_logger, blackhole::merge({std::move(attributes), {
        logging::keyword::source() = source
    }}));
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

} // namespace

void
context_t::insert(const std::string& name, std::unique_ptr<actor_t> service) {
    blackhole::scoped_attributes_t guard(*m_logger, blackhole::log::attributes_t({
        logging::keyword::source() = "core"
    }));

    const actor_t& actor = *service;

    {
        auto ptr = m_services.synchronize();

        if(std::count_if(ptr->begin(), ptr->end(), match{name})) {
            throw cocaine::error_t("service '%s' already exists", name);
        }

        service->run();

        COCAINE_LOG_INFO(m_logger, "service has been started")(
            "service", name
        );

        ptr->emplace_back(name, std::move(service));
    }

    // Fire off the signal to alert concerned subscribers about the service removal event.
    signals.service.exposed(actor);
}

std::unique_ptr<actor_t>
context_t::remove(const std::string& name) {
    blackhole::scoped_attributes_t guard(*m_logger, blackhole::log::attributes_t({
        logging::keyword::source() = "core"
    }));

    std::unique_ptr<actor_t> service;

    {
        auto ptr = m_services.synchronize();
        auto it = std::find_if(ptr->begin(), ptr->end(), match{name});

        if(it == ptr->end()) {
            throw cocaine::error_t("service '%s' doesn't exist", name);
        }

        service = std::move(it->second);
        service->terminate();

        COCAINE_LOG_INFO(m_logger, "service has been stopped")(
            "service", name
        );

        ptr->erase(it);
    }

    // Fire off the signal to alert concerned subscribers about the service insertion event.
    signals.service.removed(*service);

    return service;
}

boost::optional<const actor_t&>
context_t::locate(const std::string& name) const {
    auto ptr = m_services.synchronize();
    auto it = std::find_if(ptr->begin(), ptr->end(), match{name});

    return boost::optional<const actor_t&>(it != ptr->end(), *it->second);
}

namespace {

struct utilization_t {
    typedef std::unique_ptr<execution_unit_t> value_type;

    bool
    operator()(const value_type& lhs, const value_type& rhs) const {
        return lhs->utilization() < rhs->utilization();
    }
};

} // namespace

execution_unit_t&
context_t::engine() {
    return **std::min_element(m_pool.begin(), m_pool.end(), utilization_t());
}

void
context_t::bootstrap() {
    blackhole::scoped_attributes_t guard(*m_logger, blackhole::log::attributes_t({
        logging::keyword::source() = "core"
    }));

    COCAINE_LOG_INFO(m_logger, "initializing the core");

    m_repository = std::make_unique<api::repository_t>(*m_logger);

#ifdef COCAINE_ALLOW_RAFT
    m_raft = std::make_unique<raft::repository_t>(*this);
#endif

    // Load the builtin plugins.
    essentials::initialize(*m_repository);

    // Load the rest of plugins.
    m_repository->load(config.path.plugins);

    COCAINE_LOG_INFO(m_logger, "starting %d execution unit(s)", config.network.pool);

    while(m_pool.size() != config.network.pool) {
        m_pool.emplace_back(std::make_unique<execution_unit_t>(*this));
    }

    COCAINE_LOG_INFO(m_logger, "starting %d service(s)", config.services.size());

    std::vector<std::string> errored;

    for(auto it = config.services.begin(); it != config.services.end(); ++it) {
        blackhole::scoped_attributes_t attributes(*m_logger, {
            blackhole::attribute::make("service", it->first)
        });

        auto asio = std::make_shared<boost::asio::io_service>();

        COCAINE_LOG_INFO(m_logger, "starting service");

        try {
            insert(it->first, std::make_unique<actor_t>(*this, asio, get<api::service_t>(
                it->second.type,
               *this,
               *asio,
                it->first,
                it->second.args
            )));
        } catch(const std::exception& e) {
            COCAINE_LOG_ERROR(m_logger, "unable to initialize service: %s", e.what());
            errored.push_back(it->first);
        } catch(...) {
            COCAINE_LOG_ERROR(m_logger, "unable to initialize service");
            errored.push_back(it->first);
        }
    }

    if(!errored.empty()) {
        std::ostringstream stream;
        std::ostream_iterator<std::string> builder(stream, ", ");

        std::copy(errored.begin(), errored.end(), builder);

        COCAINE_LOG_ERROR(m_logger, "coudn't start %d service(s): %s", errored.size(), stream.str());

        signals.shutdown();

        while(!m_services->empty()) {
            m_services->front().second->terminate();
            m_services->pop_front();
        }

        COCAINE_LOG_ERROR(m_logger, "emergency core shutdown");

        throw cocaine::error_t("couldn't start %d service(s)", errored.size());
    }
}
