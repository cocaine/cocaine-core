#pragma once

#include <functional>
#include <string>
#include <system_error>

#include <asio/io_service.hpp>
#include <asio/posix/stream_descriptor.hpp>

#include "cocaine/api/isolate.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/detail/unique_id.hpp"

namespace cocaine { namespace engine {

struct manifest_t;
struct profile_t;

}} // namespace cocaine::engine

namespace cocaine {

struct drone_data {
    typedef std::function<void(const std::string&)> output_callback;

    std::string id;
    const engine::manifest_t& manifest;
    const engine::profile_t& profile;
    output_callback output;

    drone_data(const engine::manifest_t& manifest,
               const engine::profile_t& profile,
               output_callback output) :
        id(unique_id_t().string()),
        manifest(manifest),
        profile(profile),
        output(std::move(output))
    {}
};

/// Штука, которая будет уметь запускать воркеры и олицетворяющая единственный экземпляр воркера.
// TODO: Rename to `comrade`.
class drone_t : public std::enable_shared_from_this<drone_t> {
    drone_data d;
    std::unique_ptr<logging::log_t> log;
    std::unique_ptr<api::handle_t> handle;

    std::array<char, 4096> buffer;
    asio::posix::stream_descriptor watcher;

public:
    /// Spawn, connect, prepare. Instead of constructor.
    // TODO: Make async.
    static
    std::shared_ptr<drone_t>
    make(context_t& context, drone_data data, std::shared_ptr<asio::io_service> loop);

    drone_t(context_t& context, drone_data data, std::shared_ptr<asio::io_service> loop);

    /// Performs hard shutdown.
    ~drone_t();

    /// Soft shutdown, cancels all timers and watchers.
    void
    terminate();

    /// Returns statistical info (CPU load, mem, network, ...)
    // stats_t stats() const;

    /// Send to stdin.
    // void communicate(const char*, size_t);

    void
    watch();

    void
    on_watch(const std::error_code& ec, size_t len);
};

}
