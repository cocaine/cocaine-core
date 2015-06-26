#pragma once

#include <memory>
#include <string>

namespace cocaine {

struct slave_t;

class overseer_t;

struct slave_info {
    slave_t* slave;

    constexpr
    slave_info():
        slave(nullptr)
    {}

    constexpr
    slave_info(slave_t* slave):
        slave(slave)
    {}

    operator bool() const {
        return slave != nullptr;
    }
};

class balancer_t {
protected:
    std::shared_ptr<overseer_t> overseer;

public:
    explicit balancer_t(std::shared_ptr<overseer_t> overseer):
        overseer(std::move(overseer))
    {}

    virtual
    ~balancer_t() {}

    /// Called on slave spawn.
    ///
    /// Here is the right time to rebalance accumulated events from the queue.
    virtual
    void
    on_slave_spawn(const std::string& uuid) = 0;

    /// Called after slave's death.
    virtual
    void
    on_slave_death(const std::string& uuid) = 0;

    /// Called on new event.
    virtual
    slave_info
    on_request(const std::string& event, const std::string& id) = 0;

    /// Called on new channel appended into the queue.
    ///
    /// Here is the right place to review slave count and possibly spawn more.
    virtual
    void
    on_queue() = 0;

    /// Called on channel attaching.
    virtual
    void
    on_channel_started(const std::string& uuid, std::uint64_t channel) = 0;

    /// Called on channel revoking.
    virtual
    void
    on_channel_finished(const std::string& uuid, std::uint64_t channel) = 0;
};

}
