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

#include "cocaine/detail/runtime/logging.hpp"

#include "cocaine/common.hpp"

#include <blackhole/record.hpp>
#include <blackhole/sink.hpp>
#include <blackhole/sink/console.hpp>
#include <blackhole/termcolor.hpp>

namespace blackhole {

auto
factory<cocaine::logging::console_t>::type() const noexcept -> const char* {
    return "console";
}

auto
factory<cocaine::logging::console_t>::from(const config::node_t&) const -> std::unique_ptr<sink_t> {
    return blackhole::builder<blackhole::sink::console_t>()
        .colorize(cocaine::logging::debug, termcolor_t())
        .colorize(cocaine::logging::info, termcolor_t::blue())
        .colorize(cocaine::logging::warning, termcolor_t::yellow())
        .colorize(cocaine::logging::error, termcolor_t::red())
        .stdout()
        .build();
}

}  // namespace blackhole
