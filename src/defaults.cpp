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

#include "cocaine/defaults.hpp"

using namespace cocaine;

const bool defaults::log_output                = false;
const float defaults::heartbeat_timeout        = 30.0f;
const float defaults::idle_timeout             = 600.0f;
const float defaults::startup_timeout          = 10.0f;
const float defaults::termination_timeout      = 5.0f;
const unsigned long defaults::concurrency      = 10L;
const unsigned long defaults::crashlog_limit   = 50L;
const unsigned long defaults::pool_limit       = 10L;
const unsigned long defaults::queue_limit      = 100L;

const float defaults::control_timeout          = 5.0f;
const unsigned defaults::decoder_granularity   = 256;

const std::string defaults::plugins_path       = "/usr/lib/cocaine";
const std::string defaults::runtime_path       = "/var/run/cocaine";

const std::string defaults::endpoint           = "::";

const std::string defaults::log_verbosity      = "info";
const std::string defaults::log_timestamp      = "%Y-%m-%d %H:%M:%S.%f";
