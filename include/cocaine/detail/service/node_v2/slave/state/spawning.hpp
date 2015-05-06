#pragma once

#include <asio/deadline_timer.hpp>

#include "state.hpp"

namespace cocaine {

namespace api { class handle_t; }

class state_machine_t;

class spawning_t:
    public state_t,
    public std::enable_shared_from_this<spawning_t>
{
    std::shared_ptr<state_machine_t> slave;

    asio::deadline_timer timer;

public:
    explicit
    spawning_t(std::shared_ptr<state_machine_t> slave);

    virtual
    const char*
    name() const noexcept;

    virtual
    void
    terminate(const std::error_code& ec);

    void
    spawn(unsigned long timeout);

private:
    void
    on_spawn(std::unique_ptr<api::handle_t>& handle, std::chrono::high_resolution_clock::time_point start);

    void
    on_timeout(const std::error_code& ec);
};

}
