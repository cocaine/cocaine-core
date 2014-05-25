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

#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include "cocaine/common.hpp"

#include <system_error>

namespace cocaine {

class session_t;

class execution_unit_t {
    const std::unique_ptr<logging::log_t> m_log;

    // Connections

    std::map<int, std::shared_ptr<session_t>> m_sessions;

    // I/O Reactor

    std::shared_ptr<io::reactor_t> m_reactor;
    std::unique_ptr<io::chamber_t> m_chamber;

public:
    execution_unit_t(context_t& context, const std::string& name);
   ~execution_unit_t();

    void
    attach(const std::shared_ptr<io::socket<io::tcp>>& ptr, const std::shared_ptr<io::basic_dispatch_t>& dispatch);

private:
    void
    on_connect(const std::shared_ptr<io::socket<io::tcp>>& ptr, const std::shared_ptr<io::basic_dispatch_t>& dispatch);

    void
    on_message(int fd, const io::message_t& message);

    void
    on_failure(int fd, const std::error_code& error);
};

} // namespace cocaine

#endif
