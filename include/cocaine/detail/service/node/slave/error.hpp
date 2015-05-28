#pragma once

#include <system_error>

namespace cocaine { namespace error {

enum app_errors {
    invalid_app_state = 1
};

enum overseer_errors {
    /// The queue is full.
    queue_is_full = 1,

    /// Unable to spawn more tagged slaves, because it is full.
    pool_is_full
};

enum slave_errors {
    /// The slave has failed to spawn for a timeout.spawn ms.
    spawn_timeout = 1,

    /// The slave has unable to locate the Locator service from the core.
    locator_not_found,

    /// The slave has failed to activate for a timeout.activate ms.
    ///
    /// A slave is considered active when is sends a handshake message and the runtime receives it.
    activate_timeout,

    /// The slave has failed to activate, because of some errors.
    unknown_activate_error,

    /// The slave has failed to terminate itself for a timeout.terminate ms.
    teminate_timeout,

    /// The slave hasn't sent a heartbeat message during a timeout.heartbeat ms.
    heartbeat_timeout,

    /// The operation cannot be completed, because the slave is in invalid state.
    invalid_state,

    /// Unexpected IPC error occurred between the runtime and worker.
    ///
    /// In this case we cannot control the worker anymore, so the only way to handle this error - is
    /// to mark the worker as broken and to terminate it using TERM or KILL signals.
    conrol_ipc_error,

    /// The overseer is shutdowning.
    overseer_shutdowning,

    /// The slave has committed suicide, i.e. sent a terminated message.
    ///
    /// In this case it is free to do anything, for example, kill itself. We can only mark the slave
    /// as closed.
    committed_suicide,

    /// The slave has no active channels for a timeout.idle ms.
    slave_idle
};

const std::error_category&
overseer_category();

std::error_code
make_error_code(overseer_errors err);

const std::error_category&
slave_category();

std::error_code
make_error_code(slave_errors err);

}} // namespace cocaine::error

namespace std {

/// Extends the type trait std::is_error_code_enum to identify `overseer_errors` error codes.
template<>
struct is_error_code_enum<cocaine::error::overseer_errors>:
    public true_type
{};

/// Extends the type trait std::is_error_code_enum to identify `slave_errors` error codes.
template<>
struct is_error_code_enum<cocaine::error::slave_errors>:
    public true_type
{};

} // namespace std
