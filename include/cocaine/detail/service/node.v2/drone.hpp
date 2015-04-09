#pragma once

#include <functional>
#include <string>
#include <system_error>

#include <boost/variant/variant.hpp>

#include <asio/io_service.hpp>
#include <asio/deadline_timer.hpp>
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

template<class T>
class result : public boost::variant<T, std::error_code> {
public:
    result(T val) : boost::variant<T, std::error_code>(std::move(val)) {}
    result(std::error_code err) : boost::variant<T, std::error_code>(std::move(err)) {}
};

template<typename T>
class cancellable {
public:
    typedef T value_type;
    typedef std::function<void(const std::error_code&, std::shared_ptr<value_type>)> callback_type;

    std::atomic<bool> flag;
    callback_type fn;

public:
    cancellable(callback_type fn) :
        flag(false),
        fn(std::move(fn))
    {}

    void set(std::shared_ptr<value_type> v) {
        auto before = flag.exchange(true);
        if (!before) {
            fn(std::error_code(), std::move(v));
        }
    }

    void cancel() {
        set(nullptr);
    }
};

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

namespace slave {

//class active_t;

////class sealing_t {
////    // lives only to keep open channels opened, heartbeats etc.
////    // not allow to create new channels.

////    // notify overseer when finished to process channel.
////};
class unauthenticated_t {
public:
//    active_t attach(session);

//    void cancel();
};

// Spawning state lasts ~100-3000ms.
class spawning_t {
public:
    typedef result<std::shared_ptr<unauthenticated_t>> result_type;
    typedef std::function<void(result_type)> callback_type;

private:
    callback_type fn;
    std::atomic<bool> fired;

public:
    spawning_t(callback_type fn);

    void set(result_type&& res);
    void cancel();
};

//class active_t {
////    // - output fetcher.
////    // - control (with session)
//public:
//    active_t(spawning_t&&, control_t);
////    ~active_t(); // sends terminate signal and creates waitable object (30-5).

////    void attach(session);
////    auto inject(dispatch) -> upstream;

////    // notify overseer when starts/finished to process channel.
//};

std::shared_ptr<slave::spawning_t>
spawn(std::shared_ptr<asio::io_service> loop, slave::spawning_t::callback_type fn);

}

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
