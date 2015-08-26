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

#include <algorithm>
#include <array>

#include <blackhole/record.hpp>

namespace cocaine { namespace logging {

console_t::console_t() :
    blackhole::sink::console_t()
{}

auto console_t::color(const blackhole::record_t& record) const -> blackhole::sink::color_t {
    using blackhole::sink::color_t;
    static const std::array<color_t, 4> colors{{color_t(), color_t::blue(), color_t::yellow(), color_t::red()}};

    const auto id = std::min<int>(std::max<int>(0, record.severity()), colors.size() - 1);

    return colors[id];
}

}}  // namespace cocaine::logging

namespace blackhole {

auto
factory<cocaine::logging::console_t>::type() -> const char* {
    return "console";
}

auto
factory<cocaine::logging::console_t>::from(const config::node_t&) -> cocaine::logging::console_t {
    return {};

}

}// namespace blackhole
