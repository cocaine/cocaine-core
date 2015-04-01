#pragma once

#include "cocaine/context.hpp"

namespace cocaine { namespace engine {

struct manifest_t;
struct profile_t;

}} // namespace cocaine::engine

namespace cocaine { namespace service { namespace v2 {

class app_t {
    context_t& context;

    const std::unique_ptr<logging::log_t> log;

    // Configuration.
    std::unique_ptr<const engine::manifest_t> manifest;
    std::unique_ptr<const engine::profile_t>  profile;

public:
    app_t(context_t& context, const std::string& manifest, const std::string& profile);
    ~app_t();

    app_t(const app_t& other) = delete;

    app_t& operator=(const app_t& other) = delete;

private:
    void start();
};

}}} // namespace cocaine::service::v2
