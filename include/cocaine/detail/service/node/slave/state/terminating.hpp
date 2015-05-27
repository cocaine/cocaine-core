#pragma once

#include <memory>

#include "state.hpp"

namespace cocaine {

namespace api { class handle_t; }

class state_machine_t;

class terminating_t:
    public state_t,
    public std::enable_shared_from_this<terminating_t>
{
    std::shared_ptr<state_machine_t> slave;
    std::unique_ptr<api::handle_t> handle;
    std::shared_ptr<control_t> control;
    std::shared_ptr<session_t> session;

    asio::deadline_timer timer;

public:
    terminating_t(std::shared_ptr<state_machine_t> slave,
                  std::unique_ptr<api::handle_t> handle,
                  std::shared_ptr<control_t> control,
                  std::shared_ptr<session_t> session);
    ~terminating_t();

    const char*
    name() const noexcept;

    virtual
    void
    cancel();

    virtual
    void
    terminate(const std::error_code& ec);

    void
    start(unsigned long timeout, const std::error_code& ec);

private:
    void
    on_timeout(const std::error_code& ec);
};

} // namespace cocaine
