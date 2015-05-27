#pragma once

#include <queue>

#include "cocaine/context.hpp"
#include "cocaine/idl/node.hpp"

namespace cocaine {

struct manifest_t;
struct profile_t;

class overseer_t;
class unix_actor_t;

} // namespace cocaine

namespace cocaine { namespace service {

/// Represents a single application.
///
/// Starts TCP and UNIX servers.
class app_t {
    COCAINE_DECLARE_NONCOPYABLE(app_t)

    const std::unique_ptr<logging::log_t> log;

    context_t& context;

    // Configuration.
    std::unique_ptr<const manifest_t> manifest;
    std::unique_ptr<const profile_t>  profile;

    std::shared_ptr<asio::io_service> loop;
    std::unique_ptr<unix_actor_t> engine;
    std::shared_ptr<overseer_t> overseer;

    std::unique_ptr<asio::io_service::work> work;
    boost::thread thread;

public:
    app_t(context_t& context, const std::string& manifest, const std::string& profile);
   ~app_t();
};

}} // namespace cocaine::service
