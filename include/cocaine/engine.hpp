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

#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include "cocaine/common.hpp"

#include <asio/deadline_timer.hpp>

namespace cocaine {

class session_t;

template<class Protocol>
class session;

class execution_unit_t {
    COCAINE_DECLARE_NONCOPYABLE(execution_unit_t)

    class gc_action_t;

    // Connections

    std::map<int, std::shared_ptr<session_t>> m_sessions;

    // I/O

    std::shared_ptr<asio::io_service> m_asio;
    std::unique_ptr<io::chamber_t> m_chamber;

    // Initialized here because of the dependency on the io::chamber_t's thread ID.
    const std::unique_ptr<logging::logger_t> m_log;
    metrics::registry_t& m_metrics;

    static const unsigned int kCollectionInterval = 60;

    // Collects detached sessions every kCollectionInterval seconds. Normally, session slots will be
    // reused because of system fd rotation, but for low loads this will help a bit.
    std::unique_ptr<asio::deadline_timer> m_cron;

    context_t& context;

public:
    explicit
    execution_unit_t(context_t& context);

   ~execution_unit_t();

    template<class Socket>
    std::shared_ptr<session<typename Socket::protocol_type>>
    attach(std::unique_ptr<Socket> ptr, const io::dispatch_ptr_t& dispatch);

    double
    utilization() const;
};

} // namespace cocaine

#endif
