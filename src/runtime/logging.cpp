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

#include "cocaine/detail/runtime/logging.hpp"

#include "cocaine/context/config.hpp"

#include "cocaine/tuple.hpp"

#include <blackhole/formatter/json.hpp>
#include <blackhole/frontend/files.hpp>
#include <blackhole/frontend/syslog.hpp>
#include <blackhole/scoped_attributes.hpp>
#include <blackhole/sink/socket.hpp>

BLACKHOLE_BEG_NS

namespace sink {

// Mapping trait that is called by Blackhole each time when syslog mapping is required.

template<>
struct priority_traits<cocaine::logging::priorities> {
    static
    priority_t
    map(cocaine::logging::priorities level) {
        switch (level) {
        case cocaine::logging::debug:
            return priority_t::debug;
        case cocaine::logging::info:
            return priority_t::info;
        case cocaine::logging::warning:
            return priority_t::warning;
        case cocaine::logging::error:
            return priority_t::err;
        default:
            return priority_t::debug;
        }

        return priority_t::debug;
    }
};

} // namespace sink

BLACKHOLE_END_NS

// Severity attribute converter from enumeration underlying type into string

namespace cocaine { namespace logging {

void
map_severity(blackhole::aux::attachable_ostringstream& stream, const logging::priorities& level) {
    typedef blackhole::aux::underlying_type<logging::priorities>::type underlying_type;

    static const char* describe[] = {
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR"
    };

    const auto value = static_cast<underlying_type>(level);

    if(value < static_cast<underlying_type>(sizeof(describe) / sizeof(describe[0])) && value >= 0) {
        stream << describe[value];
    } else {
        stream << value;
    }
}

}} // namespace cocaine::logging

using namespace cocaine;
using namespace cocaine::logging;

init_t::init_t(const std::map<std::string, config_t::logging_t::logger_t>& config):
    config(config)
{
    auto& repository = blackhole::repository_t::instance();

    // Available logging sinks.
    typedef boost::mpl::vector<
        blackhole::sink::stream_t,
        blackhole::sink::files_t<
            blackhole::sink::files::boost_backend_t,
            blackhole::sink::rotator_t<
                blackhole::sink::files::boost_backend_t,
                blackhole::sink::rotation::watcher::move_t
            >
        >,
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

    using blackhole::keyword::tag::severity_t;
    using blackhole::keyword::tag::timestamp_t;

    // For each logging config define mappers. Then add them into the repository.

    for(auto it = config.begin(); it != config.end(); ++it) {
        // Configure some mappings for timestamps and severity attributes.
        blackhole::mapping::value_t mapper;

        mapper.add<severity_t<logging::priorities>>(&logging::map_severity);
        mapper.add<timestamp_t>(it->second.timestamp);

        // Attach them to the logging config.
        auto  config    = it->second.config;
        auto& frontends = config.frontends;

        for(auto it = frontends.begin(); it != frontends.end(); ++it) {
            it->formatter.mapper = mapper;
        }

        // Register logger configuration with the Blackhole's repository.
        repository.add_config(config);
    }
}

std::unique_ptr<logger_t>
init_t::logger(const std::string& backend) const {
    auto& repository = blackhole::repository_t::instance();

    return std::make_unique<logger_t>(
        repository.create<logger_t>(backend, config.at(backend).verbosity)
    );
}
