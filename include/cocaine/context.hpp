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

namespace signal {
class handler_base_t;
}


class context_t {

    struct impl_t;
    struct impl_deleter_t {
        void operator()(impl_t* ptr) const;
    };
    std::unique_ptr<impl_t, impl_deleter_t> impl;

public:
    context_t(config_t config, std::unique_ptr<logging::logger_t> log);
   ~context_t();

    std::unique_ptr<logging::logger_t>
    log(const std::string& source);

    std::unique_ptr<logging::logger_t>
    log(const std::string& source, blackhole::attributes_t attributes);

    const api::repository_t&
    repository() const;

    retroactive_signal<io::context_tag>&
    signal_hub();

    const config_t&
    config() const;

    port_mapping_t&
    mapper();

    // Service API
    void
    insert(const std::string& name, std::unique_ptr<actor_t> service);

    auto
    remove(const std::string& name) -> std::unique_ptr<actor_t>;

    auto
    locate(const std::string& name) const -> boost::optional<const actor_t&>;

    // Network I/O

    auto
    engine() -> execution_unit_t&;

    void
    terminate();

private:
    void
    bootstrap();

};

} // namespace cocaine

#endif
