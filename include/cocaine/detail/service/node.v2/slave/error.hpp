#pragma once

#include <system_error>

namespace cocaine { namespace error {

enum slave_errors {
    spawn_timeout = 1,
    locator_not_found,
    activate_timeout,
    teminate_timeout,
    /// The slave hasn't sent a heartbeat message during a timeout.heartbeat milliseconds.
    heartbeat_timeout,
    unknown_activate_error,
    invalid_state,
    /// Unexpected IPC error occurred between the runtime and worker.
    ///
    /// In this case we cannot control
    /// the worker anymore, so the only way to handle this error - is to mark the worker as broken
    /// and to terminate it using TERMINATE or KILL signals.
    conrol_ipc_error,
    committed_suicide
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
