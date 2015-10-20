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

#ifndef COCAINE_EXCEPTIONS_HPP
#define COCAINE_EXCEPTIONS_HPP

#include "cocaine/format.hpp"
#include "cocaine/locked_ptr.hpp"

#include <system_error>

namespace cocaine { namespace error {

enum transport_errors {
    frame_format_error = 1,
    hpack_error,
    insufficient_bytes,
    parse_error
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
    ltdl_error,
    version_mismatch
};

enum security_errors {
    token_not_found = 1
};

auto
make_error_code(transport_errors code) -> std::error_code;

auto
make_error_code(dispatch_errors code) -> std::error_code;

auto
make_error_code(repository_errors code) -> std::error_code;

auto
make_error_code(security_errors code) -> std::error_code;

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

private:
    struct impl_type;

    static
    synchronized<std::unique_ptr<impl_type>> ptr;
};

// Generic exception

struct error_t:
    public std::system_error
{
    static const std::error_code kInvalidArgumentErrorCode;

    template<class... Args>
    error_t(const std::string& e, const Args&... args):
        std::system_error(kInvalidArgumentErrorCode, cocaine::format(e, args...))
    { }

    template<class E, class... Args,
             class = typename std::enable_if<std::is_error_code_enum<E>::value ||
                                             std::is_error_condition_enum<E>::value>::type>
    error_t(const E error_id, const std::string& e, const Args&... args):
        std::system_error(make_error_code(error_id), cocaine::format(e, args...))
    { }
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

} // namespace std

#endif
