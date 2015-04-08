#pragma once

#include <functional>
#include <string>
#include <system_error>

#include <asio/io_service.hpp>
#include <asio/posix/stream_descriptor.hpp>

#include "cocaine/api/isolate.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/detail/unique_id.hpp"

namespace cocaine {

struct manifest_t;
struct profile_t;

}

namespace cocaine {

struct slave_data {
    typedef std::function<void(const std::string&)> output_callback;

    std::string id;
    const manifest_t& manifest;
    const profile_t&  profile;
    output_callback output;

    slave_data(const manifest_t& manifest, const profile_t& profile, output_callback output) :
        id(unique_id_t().string()),
        manifest(manifest),
        profile(profile),
        output(std::move(output))
    {}
};

/// Drone - single process representation.
///  - spawns using isolate.
///  - captures inputs/outputs.
///  - can get statistics.
///  - lives until process lives and vise versa.
// TODO: Rename to `comrade`, because in Soviet Russia slave owns you!
class slave_t : public std::enable_shared_from_this<slave_t> {
    slave_data d;
    std::unique_ptr<logging::log_t> log;

    // TODO: Move stdout/stderr fetcher and stdin pusher to isolate.
    std::array<char, 4096> buffer;
    asio::posix::stream_descriptor watcher;
    std::unique_ptr<api::handle_t> handle;

public:
    /// Spawn, connect, prepare. Instead of constructor.
    // TODO: Make async.
    static
    std::shared_ptr<slave_t>
    make(context_t& context, slave_data data, std::shared_ptr<asio::io_service> loop);

    // TODO: Make private, because we create this class asynchronously using `make`.
    slave_t(context_t& context, slave_data data, std::shared_ptr<asio::io_service> loop);

    /// Performs hard shutdown.
    ~slave_t();

    //? keep session in control.
//    std::shared_ptr<control_t>
//    attach(std::shared_ptr<session_t>);

    //? keep session in control.
//    upstream_ptr_t
//    process(dispatch_ptr_t);

    /// Soft shutdown, cancels all timers and watchers.
    void
    terminate();

    /// Returns statistical info (CPU load, mem, network, ...)
    // stats_t stats() const;

    /// Send to stdin.
    // void communicate(const char*, size_t);

    // TODO: Make private, because we call this when invoking `make`.
    void
    watch();

private:
    void
    on_watch(const std::error_code& ec, size_t len);
};

}
