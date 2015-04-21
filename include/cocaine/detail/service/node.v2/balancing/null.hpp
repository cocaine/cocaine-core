#pragma once

#include "base.hpp"

namespace cocaine {

class null_balancer_t:
    public balancer_t
{
public:
    null_balancer_t():
        balancer_t(nullptr)
    {}

    void
    on_slave_spawn(const std::string&) override {}

    void
    on_slave_death(const std::string&) override {}

    slave_info
    on_request(const std::string&, const std::string&) override {}

    void
    on_queue() override {}

    std::uint64_t
    on_channel_started(const std::string&) override {}

    void
    on_channel_finished(const std::string&, std::uint64_t) override {}
};

}
