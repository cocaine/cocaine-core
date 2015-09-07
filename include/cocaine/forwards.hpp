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

#ifndef COCAINE_FORWARDS_HPP
#define COCAINE_FORWARDS_HPP

#include <blackhole/forwards.hpp>

namespace cocaine {

class context_t;

template<class> class dispatch;
template<class> class upstream;

class dynamic_t;

typedef unsigned short port_t;

} // namespace cocaine

namespace cocaine { namespace api {

struct cluster_t;
struct gateway_t;
struct isolate_t;
struct service_t;
struct storage_t;

}} // namespace cocaine::api

namespace cocaine { namespace io {

// I/O threads

class chamber_t;

// I/O streams

template<class, class>
class readable_stream;

template<class, class>
class writable_stream;

// Stream composition

struct encoder_t;
struct decoder_t;

template<class, class = encoder_t, class = decoder_t>
struct transport;

// Generic RPC objects

class basic_dispatch_t;
class basic_upstream_t;

typedef std::shared_ptr<const basic_dispatch_t> dispatch_ptr_t;
typedef std::shared_ptr<      basic_upstream_t> upstream_ptr_t;

template<class>
struct protocol;

}} // namespace cocaine::io

namespace cocaine { namespace logging {

enum priorities: int {
    debug   =  0,
    info    =  1,
    warning =  2,
    error   =  3
};

template<class Wrapped, class AttributeFetcher>
class dynamic_wrapper;

struct trace_attribute_fetcher_t;

typedef blackhole::verbose_logger_t<logging::priorities> logger_t;
typedef dynamic_wrapper<logger_t, trace_attribute_fetcher_t> log_t;

}} // namespace cocaine::logging

// Third-party forwards

namespace asio {

class io_service;

} // namespace asio

#endif
