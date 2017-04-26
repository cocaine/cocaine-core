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

#include <memory>
#include <vector>

// Third-party forwards

namespace asio {

class io_service;

namespace ip {

class tcp;

} // namespace ip

namespace local {

class stream_protocol;

} // namespace local

} // namespace asio

namespace blackhole {
inline namespace v1 {

class logger_t;
class severity_t;

}  // namespace v1
}  // namespace blackhole

namespace metrics {

class registry_t;

}  // namespace metrics

namespace cocaine {

class actor_t;

template<typename Protocol>
class actor;

class tcp_actor_t;

struct config_t;
class context_t;
class dynamic_t;
class execution_unit_t;
class port_mapping_t;
class session_t;
class trace_t;
class filter_t;

template<class> class dispatch;
template<class> class upstream;
template<class> class retroactive_signal;

typedef unsigned short port_t;

} // namespace cocaine

namespace cocaine { namespace api {

class authentication_t;
class repository_t;
class unicorn_t;
class unicorn_scope_t;

struct cluster_t;
struct gateway_t;
struct isolate_t;
struct service_t;
struct storage_t;

}} // namespace cocaine::api

namespace cocaine { namespace auth {

class identity_t;

}} // namespace cocaine::auth

namespace cocaine { namespace authorization {

class event_t;
class unicorn_t;
class storage_t;

}} // namespace cocaine::authorization

namespace cocaine { namespace context {

struct quote_t;

}} // namespace cocanie::context

namespace cocaine { namespace hpack {

class header_t;
struct headers;
struct header_static_table;

typedef std::vector<header_t> header_storage_t;

}} // namespace cocaine::hpack

namespace cocaine { namespace io {

// Used for context signals.
struct context_tag;

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

typedef std::shared_ptr<basic_dispatch_t> dispatch_ptr_t;
typedef std::shared_ptr<basic_upstream_t> upstream_ptr_t;

template<class>
struct protocol;

}} // namespace cocaine::io

namespace cocaine { namespace io { namespace aux {

struct decoded_message_t;
struct encoded_message_t;

}}} // namespace cocaine::io::aux

namespace cocaine { namespace logging {

enum priorities: int {
    debug   =  0,
    info    =  1,
    warning =  2,
    error   =  3
};

// Import the logger in our namespace.
using blackhole::logger_t;

}} // namespace cocaine::logging

namespace cocaine { namespace unicorn {

class versioned_value_t;

}} // namespace cocaine::unicorn

#endif
