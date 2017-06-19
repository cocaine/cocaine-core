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

#ifndef COCAINE_EXCEPTIONS_HPP
#define COCAINE_EXCEPTIONS_HPP

#include "cocaine/format.hpp"

#include <memory>
#include <system_error>

namespace cocaine { namespace error {

enum transport_errors {
    frame_format_error = 1,
    // TODO: maybe this should belong to protocol error, but it's here for backward compatibility
    hpack_error,
    insufficient_bytes,
    parse_error
};

enum protocol_errors {
    closed_upstream = 1
};

enum dispatch_errors {
    duplicate_slot = 1,
    invalid_argument,
    not_connected,
    revoked_channel,
    slot_not_found,
    unbound_dispatch,
    uncaught_error
};

enum repository_errors {
    component_not_found = 1,
    duplicate_component,
    initialization_error,
    invalid_interface,
    dlopen_error,
    version_mismatch,
    component_not_registered
};

enum security_errors {
    token_not_found = 1,
    unauthorized,
    permission_denied,
    permissions_changed,
    invalid_acl_framing
};

enum locator_errors {
    service_not_available = 1,
    routing_storage_error,
    missing_version_error,
    gateway_duplicate_service,
    gateway_missing_service
};

enum unicorn_errors {
    child_not_allowed = 1,
    invalid_type,
    invalid_value,
    unknown_error,
    invalid_node_name,
    invalid_path,
    version_not_allowed,
    no_node,
    node_exists,
    connection_loss,
    backend_internal_error
};

auto
make_error_code(transport_errors code) -> std::error_code;

auto
make_error_code(protocol_errors code) -> std::error_code;

auto
make_error_code(dispatch_errors code) -> std::error_code;

auto
make_error_code(repository_errors code) -> std::error_code;

auto
make_error_code(security_errors code) -> std::error_code;

auto
make_error_code(locator_errors code) -> std::error_code;

auto
make_error_code(unicorn_errors code) -> std::error_code;

// Error categories registrar

struct registrar {
    static
    auto
    map(const std::error_category& ec) -> size_t;

    static
    auto
    map(size_t id) -> const std::error_category&;

    // Modifiers

    static
    auto
    add(const std::error_category& ec) -> size_t;

    static
    auto
    add(const std::error_category& ec, size_t index) -> void;

private:
    class storage_type;
    static auto instance() -> storage_type&;
};

// Generic exception

struct error_t:
    public std::system_error
{
    static const std::error_code kInvalidArgumentErrorCode;

    template<class... Args>
    error_t(const std::string& fmt, const Args&... args):
        std::system_error(kInvalidArgumentErrorCode, cocaine::format(fmt, args...))
    {}

    template<class... Args>
    error_t(std::error_code ec, const std::string& fmt, const Args&... args):
        std::system_error(std::move(ec), cocaine::format(fmt, args...))
    {}

    template<class E, class... Args, class = typename std::enable_if<
        std::is_error_code_enum<E>::value || std::is_error_condition_enum<E>::value
    >::type>
    error_t(const E err, const std::string& fmt, const Args&... args):
        std::system_error(make_error_code(err), cocaine::format(fmt, args...))
    {}
};

std::string
to_string(const std::system_error& e);

} // namespace error

// For backward-compatibility with fucking computers.
using error::error_t;

} // namespace cocaine

namespace std {

template<>
struct is_error_code_enum<cocaine::error::transport_errors>:
    public true_type
{ };

template<>
struct is_error_code_enum<cocaine::error::protocol_errors>:
    public true_type
{ };

template<>
struct is_error_code_enum<cocaine::error::dispatch_errors>:
    public true_type
{ };

template<>
struct is_error_code_enum<cocaine::error::repository_errors>:
    public true_type
{ };

template<>
struct is_error_code_enum<cocaine::error::security_errors>:
    public true_type
{ };

template<>
struct is_error_code_enum<cocaine::error::locator_errors>:
public true_type
{ };


template<>
struct is_error_code_enum<cocaine::error::unicorn_errors>:
    public true_type
{ };

} // namespace std

#endif
