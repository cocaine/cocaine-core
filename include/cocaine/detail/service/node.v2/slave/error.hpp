#pragma once

#include <system_error>

namespace cocaine { namespace error {

enum slave_errors {
    spawn_timeout = 1,
    locator_not_found,
    activate_timeout,
    invalid_state
};

const std::error_category&
slave_category();

std::error_code
make_error_code(slave_errors err);

}} // namespace cocaine::error

namespace std {

/// Extends the type trait std::is_error_code_enum to identify `slave_errors` error codes.
template<>
struct is_error_code_enum<cocaine::error::slave_errors>:
    public true_type
{};

} // namespace std
