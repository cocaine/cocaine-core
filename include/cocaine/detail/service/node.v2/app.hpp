#pragma once

#include "cocaine/context.hpp"

namespace cocaine {

struct manifest_t;
struct profile_t;

} // namespace cocaine

namespace cocaine {

class unix_actor_t;
class overseer_t;
class slave_t;
class control_t;

}

namespace cocaine { namespace service { namespace v2 {

class balancer_t {
public:
    virtual
    void rebalance() = 0;
};

class app_t {
    context_t& context;

    const std::unique_ptr<logging::log_t> log;

    // Configuration.
    std::unique_ptr<const manifest_t> manifest;
    std::unique_ptr<const profile_t>  profile;

    std::shared_ptr<asio::io_service> loop;
    std::unique_ptr<unix_actor_t> engine;
    std::unique_ptr<balancer_t> balancer;
    std::shared_ptr<overseer_t> overseer;

public:
    app_t(context_t& context, const std::string& manifest, const std::string& profile);
    ~app_t();

    app_t(const app_t& other) = delete;

    app_t& operator=(const app_t& other) = delete;

private:
    void start();
};

}}} // namespace cocaine::service::v2
