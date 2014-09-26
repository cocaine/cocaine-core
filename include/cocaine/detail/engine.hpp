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

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace cocaine {

class session_t;

class execution_unit_t {
    COCAINE_DECLARE_NONCOPYABLE(execution_unit_t)

    std::unique_ptr<logging::log_t> m_log;

    // Connections

    std::map<int, std::shared_ptr<session_t>> m_sessions;

    // I/O

    std::shared_ptr<boost::asio::io_service> m_asio;
    std::unique_ptr<io::chamber_t> m_chamber;

public:
    explicit
    execution_unit_t(context_t& context);

   ~execution_unit_t();

    void
    attach(const std::shared_ptr<boost::asio::ip::tcp::socket>& ptr, const io::dispatch_ptr_t& dispatch);

    double
    utilization() const;

private:
    void
    attach_impl(const std::shared_ptr<boost::asio::ip::tcp::socket>& ptr, const io::dispatch_ptr_t& dispatch);

    void
    detach(int fd);

    void
    on_shutdown(const boost::system::error_code& ec, int fd);
};

} // namespace cocaine

#endif
