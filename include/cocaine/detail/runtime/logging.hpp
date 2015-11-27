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

#ifndef COCAINE_BOOTSTRAP_LOGGING_HPP
#define COCAINE_BOOTSTRAP_LOGGING_HPP

#include <blackhole/sink/console.hpp>

namespace cocaine { namespace logging {

class console_t : public blackhole::sink::console_t {
public:
    console_t();

protected:
    auto color(const blackhole::record_t& record) const -> blackhole::sink::color_t;
};

}} // namespace cocaine::logging

namespace blackhole {
inline namespace v1 {
namespace config {

class node_t;

}  // namespace config
}  // namespace v1
}  // namespace blackhole

namespace blackhole {
inline namespace v1 {

template<typename>
struct factory;

template<>
struct factory<cocaine::logging::console_t> {
    static auto type() -> const char*;
    static auto from(const config::node_t& config) -> cocaine::logging::console_t;
};

}  // namespace v1
}  // namespace blackhole

#endif
