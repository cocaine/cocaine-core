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
#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

#include "slave/control.hpp"

namespace cocaine {

class control_t;

}

namespace cocaine {

template<class T>
class result : public boost::variant<T, std::error_code> {
public:
    result(T val) : boost::variant<T, std::error_code>(std::move(val)) {}
    result(std::error_code err) : boost::variant<T, std::error_code>(std::move(err)) {}
};

struct slave_data {
    typedef std::function<void(const std::string&)> output_callback;

    std::string id;
    manifest_t manifest;
    profile_t  profile;
    output_callback output;

    slave_data(manifest_t manifest, profile_t profile, output_callback output) :
        id(unique_id_t().string()),
        manifest(manifest),
        profile(profile),
        output(std::move(output))
    {}
};

// TODO: Make an owners, not shared pointers.
namespace slave {

//class sealing_t {
//    // lives only to keep open channels opened, heartbeats etc.
//    // not allow to create new channels.

//    // notify overseer when finished to process channel.
//};

class unauthenticated_t;

// Spawning state lasts up to 3000ms.
class spawning_t {
public:
    typedef result<std::shared_ptr<unauthenticated_t>> result_type;
    typedef std::function<void(result_type)> callback_type;

private:
    std::unique_ptr<logging::log_t> log;

    callback_type fn;
    std::atomic<bool> fired;

public:
    spawning_t(context_t& context, slave_data d, std::shared_ptr<asio::io_service> loop, callback_type fn);

    void set(result_type&& res);
    void cancel();
};

class active_t;

class fetcher_t;

class unauthenticated_t : public std::enable_shared_from_this<unauthenticated_t> {
    friend class spawning_t;
    friend class active_t;

    std::unique_ptr<logging::log_t> log;

    slave_data d;

    std::shared_ptr<fetcher_t> fetcher;
    std::unique_ptr<api::handle_t> handle;

public:
    unauthenticated_t(context_t& context, slave_data d, std::shared_ptr<asio::io_service> loop, std::unique_ptr<api::handle_t> handle);

    // This method invalidates current object.
    std::shared_ptr<active_t> activate(std::shared_ptr<control_t> control);

    void terminate();
};

class active_t {
//    // - output fetcher.
//    // - control (with session)
public:
    active_t(unauthenticated_t&&);
//    ~active_t(); // sends terminate signal and creates waitable object (30-5).

//    void attach(session);
//    auto inject(dispatch) -> upstream;

//    // notify overseer when starts/finished to process channel.
};

std::shared_ptr<slave::spawning_t>
spawn(context_t& context, slave_data d, std::shared_ptr<asio::io_service> loop, slave::spawning_t::callback_type fn);

}

/// Drone - single process representation.
///  - spawns using isolate.
///  - captures inputs/outputs.
///  - can get statistics.
///  - lives until process lives and vise versa.
// TODO: Rename to `comrade`, because in Soviet Russia slave owns you!

}
