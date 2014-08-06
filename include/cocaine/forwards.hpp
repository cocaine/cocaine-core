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

} // namespace cocaine

namespace cocaine { namespace api {

struct gateway_t;
struct isolate_t;
struct service_t;
struct storage_t;

}} // namespace cocaine::api

namespace cocaine { namespace io {

struct local;
struct tcp;
struct udp;

template<class>
struct socket;

template<class>
struct acceptor;

// I/O privimites

struct reactor_t;
struct chamber_t;

template<class>
struct connector;

template<class>
struct readable_stream;

template<class>
struct writable_stream;

struct timeout_t;

// RPC primitives

template<class>
struct protocol;

struct message_t;

class basic_dispatch_t;
class basic_upstream_t;

// Messaging

template<class>
class encoder;

template<class>
class decoder;

template<class>
struct channel;

}} // namespace cocaine::io

namespace cocaine { namespace logging {

enum priorities: int {
    debug,
    info,
    warning,
    error
};

typedef blackhole::verbose_logger_t<logging::priorities> logger_t;
typedef blackhole::wrapper_t<logger_t> log_t;

}} // namespace cocaine::logging

namespace cocaine { namespace raft {

class repository_t;

}} // namesapce cocaine::raft

#endif
