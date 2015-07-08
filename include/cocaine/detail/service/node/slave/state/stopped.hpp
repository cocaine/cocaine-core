#pragma once

#include "state.hpp"

namespace cocaine {

class stopped_t:
    public state_t
{
    std::error_code ec;

public:
    explicit
    stopped_t(std::error_code ec);

    virtual
    void
    cancel();

    virtual
    const char*
    name() const noexcept;
};

} // namespace cocaine
