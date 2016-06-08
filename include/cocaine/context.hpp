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

#ifndef COCAINE_CONTEXT_HPP
#define COCAINE_CONTEXT_HPP

#include "cocaine/common.hpp"

#include <blackhole/attributes.hpp>

#include <boost/optional/optional_fwd.hpp>

namespace cocaine {

// Context

class context_t {
public:
    virtual
    ~context_t() {}

    virtual
    std::unique_ptr<logging::logger_t>
    log(const std::string& source) = 0;

    virtual
    std::unique_ptr<logging::logger_t>
    log(const std::string& source, blackhole::attributes_t attributes) = 0;

    virtual
    void
    logger_filter(filter_t new_filter) = 0;

    virtual
    const api::repository_t&
    repository() const = 0;

    virtual
    retroactive_signal<io::context_tag>&
    signal_hub() = 0;

    virtual
    metrics::registry_t&
    metrics_hub() = 0;

    virtual
    const config_t&
    config() const = 0;

    virtual
    port_mapping_t&
    mapper() = 0;

    // Service API
    virtual
    void
    insert(const std::string& name, std::unique_ptr<actor_t> service) = 0;

    virtual
    auto
    remove(const std::string& name) -> std::unique_ptr<actor_t> = 0;

    virtual
    auto
    locate(const std::string& name) const -> boost::optional<const actor_t&> = 0;

    // Network I/O

    virtual
    auto
    engine() -> execution_unit_t& = 0;

    virtual
    void
    terminate() = 0;
};

std::unique_ptr<context_t>
make_context(std::unique_ptr<config_t> config, std::unique_ptr<logging::logger_t> log);

} // namespace cocaine

#endif
