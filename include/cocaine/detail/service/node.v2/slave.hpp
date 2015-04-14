#pragma once

#include <functional>
#include <string>
#include <system_error>

#include <boost/circular_buffer.hpp>
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

#include "cocaine/detail/service/node.v2/result.hpp"
#include "cocaine/detail/service/node.v2/slave/control.hpp"
#include "cocaine/detail/service/node.v2/splitter.hpp"

namespace cocaine {

struct slave_context {
    context_t&  context;
    manifest_t  manifest;
    profile_t   profile;
    std::string id;

    slave_context(context_t& context, manifest_t manifest, profile_t profile) :
        context(context),
        manifest(manifest),
        profile(profile),
        id(unique_id_t().string())
    {}
};

// TODO: Rename to `comrade`, because in Soviet Russia slave owns you!
class slave_t {
public:
    typedef std::function<void(const std::error_code&)> cleanup_handler;

private:
    const std::unique_ptr<logging::log_t> log;

    const slave_context context;
    asio::io_service& loop;
    const cleanup_handler cleanup;

    class fetcher_t;

    class state_t;
    class spawning_t;
    class unauthenticated_t;
    class broken_t;

    splitter_t splitter;
    std::shared_ptr<fetcher_t> fetcher;
    boost::circular_buffer<std::string> lines;

    synchronized<std::shared_ptr<state_t>> state;

public:
    slave_t(slave_context ctx, asio::io_service& loop, cleanup_handler fn);

    ~slave_t();

    slave_t(const slave_t& other) = delete;
    slave_t(slave_t&& other) = delete;

    slave_t& operator=(const slave_t& other) = delete;
    slave_t& operator=(slave_t&& other) = delete;

    void
    activate(std::shared_ptr<session_t> session, std::shared_ptr<control_t> control);

private:
    void
    output(const char* data, size_t size);

    void
    migrate(std::shared_ptr<state_t> desired);

    void
    close(std::error_code ec);
};

}
