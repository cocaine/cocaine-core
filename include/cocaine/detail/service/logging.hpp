/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_LOGGING_SERVICE_HPP
#define COCAINE_LOGGING_SERVICE_HPP

#include "cocaine/api/service.hpp"

#include "cocaine/idl/logging.hpp"
#include "cocaine/rpc/dispatch.hpp"

namespace cocaine { namespace service {

class logging_t:
    public api::service_t,
    public dispatch<io::log_tag>
{
    std::unique_ptr<logging::logger_t> logger;
    std::unique_ptr<logging::log_t>    wrapper;

public:
    logging_t(context_t& context, asio::io_service& asio, const std::string& name, const dynamic_t& args);

    virtual
    auto
    prototype() const -> const io::basic_dispatch_t&;

private:
    void
    on_emit(logging::priorities level, std::string source, std::string message,
            blackhole::attribute::set_t attributes);

    auto
    on_verbosity() const -> logging::priorities;
};

}} // namespace cocaine::service

#endif
