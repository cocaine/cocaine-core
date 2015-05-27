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
    on_slave_spawn(const std::string&) {}

    void
    on_slave_death(const std::string&) {}

    slave_info
    on_request(const std::string&, const std::string&) {}

    void
    on_queue() {}

    void
    on_channel_started(const std::string&, std::uint64_t) {}

    void
    on_channel_finished(const std::string&, std::uint64_t) {}
};

} // namespace cocaine
