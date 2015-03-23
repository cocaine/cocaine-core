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

#include "cocaine/context/config.hpp"
#include "cocaine/context/mapper.hpp"
#include "cocaine/context/signal.hpp"

#include "cocaine/locked_ptr.hpp"
#include "cocaine/repository.hpp"

#include <blackhole/blackhole.hpp>

#define BOOST_BIND_NO_PLACEHOLDERS
#include <boost/optional.hpp>
#include <boost/signals2/signal.hpp>

namespace cocaine {

// Context

namespace signals = boost::signals2;

class actor_t;
class execution_unit_t;

class context_t {
    COCAINE_DECLARE_NONCOPYABLE(context_t)

    typedef std::deque<std::pair<std::string, std::unique_ptr<actor_t>>> service_list_t;

    // TODO: There was an idea to use the Repository to enable pluggable sinks and whatever else for
    // for the Blackhole, when all the common stuff is extracted to a separate library.
    std::unique_ptr<logging::logger_t> m_logger;

    // NOTE: This is the first object in the component tree, all the other dynamic components, be it
    // storages or isolates, have to be declared after this one.
    std::unique_ptr<api::repository_t> m_repository;

    // A pool of execution units - threads responsible for doing all the service invocations.
    std::vector<std::unique_ptr<execution_unit_t>> m_pool;

    // Services are stored as a vector of pairs to preserve the initialization order. Synchronized,
    // because services are allowed to start and stop other services during their lifetime.
    synchronized<service_list_t> m_services;

public:
    const config_t config;

    // Service port mapping and pinning.
    port_mapping_t mapper;

    struct signals_t {
        typedef signals::signal<void()> context_signals_t;
        typedef cocaine::retroactive_signal<void(const actor_t& service)> service_signals_t;

        struct {
            // Fired on service creation, after service's thread is launched and is ready to accept
            // and process new incoming connections.
            service_signals_t exposed;

            // Fired on service destruction, after the service was removed from its endpoints, but
            // before the service object is actually destroyed.
            service_signals_t removed;
        } service;

        // Fired first thing on context shutdown. This is a very good time to cleanup persistent
        // connections, synchronize disk state and so on.
        context_signals_t shutdown;
    };

    // Lifecycle management signals.
    signals_t signals;

public:
    context_t(config_t config, std::unique_ptr<logging::logger_t> logger);
   ~context_t();

    std::unique_ptr<logging::log_t>
    log(const std::string& source, blackhole::attribute::set_t = blackhole::attribute::set_t());

    template<class Category, typename... Args>
    typename api::category_traits<Category>::ptr_type
    get(const std::string& type, Args&&... args) const;

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

private:
    void
    bootstrap();
};

template<class Category, typename... Args>
typename api::category_traits<Category>::ptr_type
context_t::get(const std::string& type, Args&&... args) const {
    return m_repository->get<Category>(type, std::forward<Args>(args)...);
}

} // namespace cocaine

#endif
