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

#include <blackhole/formatter/json.hpp>
#include <blackhole/frontend/files.hpp>
#include <blackhole/frontend/syslog.hpp>
#include <blackhole/scoped_attributes.hpp>
#include <blackhole/sink/socket.hpp>

#include <boost/spirit/include/karma_char.hpp>
#include <boost/spirit/include/karma_generate.hpp>
#include <boost/spirit/include/karma_list.hpp>
#include <boost/spirit/include/karma_string.hpp>

using namespace cocaine;

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
    repository.registrate<sinks_t, formatters_t>();

    try {
        using blackhole::keyword::tag::timestamp_t;
        using blackhole::keyword::tag::severity_t;

        // For each logging config define mappers. Then add them into the repository.
        for(auto it = config.logging.loggers.begin(); it != config.logging.loggers.end(); ++it) {
            // Configure some mappings for timestamps and severity attributes.
            blackhole::mapping::value_t mapper;

            mapper.add<severity_t<logging::priorities>>(&logging::map_severity);
            mapper.add<timestamp_t>(it->second.timestamp);

            // Attach them to the logging config.
            auto config = it->second.config;
            auto& frontends = config.frontends;

            for(auto it = frontends.begin(); it != frontends.end(); ++it) {
                it->formatter.mapper = mapper;
            }

            // Register logger configuration with the Blackhole's repository.
            repository.add_config(config);
        }

        typedef logging::logger_t logger_type;

        // Fetch 'core' configuration object.
        auto logger = config.logging.loggers.at(logger_backend);

        // Try to initialize the logger. If it fails, there's no way to report the failure, except
        // printing it to the standart output.
        m_logger = std::make_unique<logger_type>(
            repository.create<logger_type>(logger_backend, logger.verbosity)
        );
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
        blackhole::attribute::set_t({logging::keyword::source() = "core"})
    );

    COCAINE_LOG_INFO(m_logger, "stopping %d service(s)", m_services->size());

    // Fire off to alert concerned subscribers about the shutdown. This signal happens before all
    // the outstanding connections are closed, so services have a chance to send their last wishes.
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
context_t::log(const std::string& source, blackhole::attribute::set_t attributes) {
    attributes.emplace_back(logging::keyword::source() = source);
    return std::make_unique<logging::log_t>(*m_logger, std::move(attributes));
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
    blackhole::scoped_attributes_t guard(*m_logger, blackhole::attribute::set_t({
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
    blackhole::scoped_attributes_t guard(*m_logger, blackhole::attribute::set_t({
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
    auto it  = std::find_if(ptr->begin(), ptr->end(), match{name});

    if(it == ptr->end()) {
        return boost::none;
    }

    return boost::optional<const actor_t&>(it->second->is_active(), *it->second);
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
    blackhole::scoped_attributes_t guard(*m_logger, blackhole::attribute::set_t({
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

        const auto asio = std::make_shared<asio::io_service>();

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
        std::ostream_iterator<char> builder(stream);

        boost::spirit::karma::generate(builder, boost::spirit::karma::string % ", ", errored);

        COCAINE_LOG_ERROR(m_logger, "coudn't start %d service(s): %s", errored.size(), stream.str());

        signals.shutdown();

        while(!m_services->empty()) {
            m_services->back().second->terminate();
            m_services->pop_back();
        }

        COCAINE_LOG_ERROR(m_logger, "emergency core shutdown");

        throw cocaine::error_t("couldn't start %d service(s)", errored.size());
    }
}
