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

#ifndef COCAINE_BOOTSTRAP_LOGGING_HPP
#define COCAINE_BOOTSTRAP_LOGGING_HPP

#include "cocaine/common.hpp"

#include "cocaine/context/config.hpp"

namespace cocaine { namespace logging {

class init_t {
    std::map<std::string, config_t::logging_t::logger_t> config_;

public:
    explicit
    init_t(const std::map<std::string, config_t::logging_t::logger_t>& config);

    std::unique_ptr<logger_t>
    logger(const std::string& backend) const;

    config_t::logging_t::logger_t
    config(const std::string& backend) const;
};

}} // namespace cocaine::logging

#endif
