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
#include "cocaine/trace/logger/blackhole.hpp"

#include "cocaine/trace/logger/blackhole.hpp"

#include "cocaine/trace/logger/blackhole.hpp"

#include <blackhole/blackhole.hpp>
#include <blackhole/keyword.hpp>
#include <blackhole/logger/wrapper.hpp>
#include <cassert>
#ifdef COCAINE_DEBUG
    cocaine::logging::log_t*&
    get_cocaine_dev_logger__();
    #define COCAINE_LOG_DEV_INIT(log) get_cocaine_dev_logger__() = &log;
    #define COCAINE_LOG_DEV(...) COCAINE_LOG_INFO(*(get_cocaine_dev_logger__()), __VA_ARGS__)
#else
    #define COCAINE_LOG_DEV_INIT(log) ((void)0)
    #define COCAINE_LOG_DEV(...) ((void)0)
#endif
#define COCAINE_LOG(_log_, _level_, ...) \
    if(auto _record_ = ::cocaine::logging::detail::logger_ptr(_log_)->open_record(_level_)) \
        ::blackhole::aux::logger::make_pusher(*(::cocaine::logging::detail::logger_ptr(_log_)), _record_, __VA_ARGS__)

#define COCAINE_LOG_DEBUG(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::debug, __VA_ARGS__)

#define COCAINE_LOG_INFO(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::info, __VA_ARGS__)

#define COCAINE_LOG_WARNING(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::warning, __VA_ARGS__)

#define COCAINE_LOG_ERROR(_log_, ...) \
    COCAINE_LOG(_log_, ::cocaine::logging::error, __VA_ARGS__)

#define COCAINE_LOG_ZIPKIN(_log_, ...) \
    if(!trace_t::current().empty()) COCAINE_LOG_INFO(_log_, __VA_ARGS__)

namespace cocaine { namespace logging {

DECLARE_KEYWORD(source, std::string)

namespace detail {

template<class T>
inline
const T*
logger_ptr(const T& log) {
    return &log;
}

template<class T>
inline
const T*
logger_ptr(const T* log) {
    return log;
}

template<class T>
inline
const T*
logger_ptr(const std::unique_ptr<T>& log) {
    return log.get();
}

template<class T>
inline
const T*
logger_ptr(const std::shared_ptr<T>& log) {
    return log.get();
}

} // namespace detail

// C++ typename demangling

auto
demangle(const std::string& mangled) -> std::string;

template<class T>
auto
demangle() -> std::string {
    return demangle(typeid(T).name());
}

}} // namespace cocaine::logging

#endif
