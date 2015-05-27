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

class app_t {
    context_t& context;

    const std::unique_ptr<logging::log_t> log;

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
    app_t(const app_t& other) = delete;

    ~app_t();

    app_t& operator=(const app_t& other) = delete;

private:
    void start();
};

}} // namespace cocaine::service
