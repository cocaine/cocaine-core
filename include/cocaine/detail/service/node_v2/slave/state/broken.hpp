#pragma once

#include "state.hpp"

namespace cocaine {

class broken_t:
    public state_t
{
    std::error_code ec;

public:
    explicit
    broken_t(std::error_code ec);

    virtual
    const char*
    name() const noexcept;
};

} // namespace cocaine
