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

#ifndef COCAINE_LOGGING_HPP
#define COCAINE_LOGGING_HPP

#include "cocaine/common.hpp"

#define BOOST_BIND_NO_PLACEHOLDERS
#include <blackhole/blackhole.hpp>
#include <blackhole/keyword.hpp>
#include <blackhole/logger/wrapper.hpp>

namespace cocaine { namespace logging {

DECLARE_KEYWORD(source, std::string)

// C++ typename demangling

auto
demangle(const std::string& mangled) -> std::string;

template<class T>
auto
demangle() -> std::string {
    return demangle(typeid(T).name());
}

}} // namespace cocaine::logging

#define COCAINE_LOG(_log_, _level_, ...) \
    if(auto record = (_log_)->open_record(_level_)) \
        ::blackhole::aux::logger::make_pusher(*(_log_), record, __VA_ARGS__)

#define COCAINE_LOG_DEBUG(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::debug, __VA_ARGS__)

#define COCAINE_LOG_INFO(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::info, __VA_ARGS__)

#define COCAINE_LOG_WARNING(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::warning, __VA_ARGS__)

#define COCAINE_LOG_ERROR(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::error, __VA_ARGS__)

#endif
