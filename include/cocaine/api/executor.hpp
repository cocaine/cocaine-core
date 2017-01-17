#pragma once

#include <functional>

namespace cocaine {
namespace api {

class executor_t {
public:
    typedef std::function<void() noexcept> work_t;

    virtual
    ~executor_t() = default;

    virtual
    auto spawn(work_t work) -> void = 0;
};

} // namespace api
} // namespace cocaine
