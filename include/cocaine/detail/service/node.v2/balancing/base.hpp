#pragma once

#include <string>

namespace cocaine {

struct slave_handler_t;

class overseer_t;

struct slave_info {
    slave_handler_t* slave;
    std::string id;
    std::uint64_t load;

    slave_info() :
        slave(nullptr),
        load(0)
    {}

    slave_info(slave_handler_t* slave, std::string id, std::uint64_t load) :
        slave(slave),
        id(std::move(id)),
        load(load)
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

    virtual
    void
    on_slave_spawn(const std::string& uuid) = 0;

    virtual
    void
    on_slave_death(const std::string& uuid) = 0;

    virtual
    slave_info
    on_request(const std::string& event, const std::string& id) = 0;

    virtual
    void
    on_queue() = 0;

    virtual
    std::uint64_t
    on_channel_started(const std::string& uuid) = 0;

    virtual
    void
    on_channel_finished(const std::string& uuid, std::uint64_t channel) = 0;
};

}
