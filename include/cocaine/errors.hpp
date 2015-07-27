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
#include "cocaine/locked_ptr.hpp"

#include <system_error>

namespace cocaine { namespace error {

enum transport_errors {
    frame_format_error = 1,
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
    struct impl_type;

    static
    auto
    map(const std::error_category& ec) -> size_t;

    static
    auto
    map(size_t id) -> const std::error_category&;

    // Modifiers

    static
    bool
    add(const std::error_category& ec);

private:
    static
    synchronized<std::unique_ptr<impl_type>> ptr;
};

} // namespace error

struct error_t:
    public std::system_error
{
    template<class... Args>
    error_t(const std::string& e, const Args&... args):
        std::system_error(std::make_error_code(std::errc::invalid_argument), format(e, args...))
    { }
};

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
