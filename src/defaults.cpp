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

#include "cocaine/defaults.hpp"

using namespace cocaine;

const std::string defaults::plugins_path  = "/usr/lib/cocaine";
const std::string defaults::runtime_path  = "/var/run/cocaine";

const std::string defaults::endpoint      = "::";

const std::string defaults::log_verbosity = "info";
const std::string defaults::log_timestamp = "%Y-%m-%d %H:%M:%S.%f";