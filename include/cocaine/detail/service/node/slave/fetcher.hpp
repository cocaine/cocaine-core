#pragma once

#include <array>
#include <memory>

#include "cocaine/locked_ptr.hpp"

#include <asio/posix/stream_descriptor.hpp>

namespace cocaine {

class state_machine_t;

/// The slave's output fetcher.
///
/// \reentrant all methods must be called from the event loop thread, otherwise the behavior is
/// undefined.
class fetcher_t:
    public std::enable_shared_from_this<fetcher_t>
{
    std::shared_ptr<state_machine_t> slave;

    std::array<char, 4096> buffer;

    typedef asio::posix::stream_descriptor watcher_type;
    synchronized<watcher_type> watcher;

public:
    explicit
    fetcher_t(std::shared_ptr<state_machine_t> slave);

    /// Assigns an existing native descriptor to the output watcher and starts watching over it.
    ///
    /// \throws std::system_error on any system error while assigning an fd.
    void
    assign(int fd);

    /// Cancels all asynchronous operations associated with the descriptor by closing it.
    void
    close();

private:
    void
    watch();

    void
    on_read(const std::error_code& ec, size_t len);
};

} // namespace cocaine
