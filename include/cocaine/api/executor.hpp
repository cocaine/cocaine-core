#pragma once

#include <functional>
#include <memory>
#include <type_traits>

namespace cocaine {
namespace api {

class executor_t {
    template<class F>
    class move_only_wrapper {
    public:
        move_only_wrapper(F _f):
            f(std::make_shared<F>(std::move(_f)))
        {}

        auto
        operator()() -> void {
            return (*f)();
        }

    private:
        std::shared_ptr<F> f;
    };
public:
    typedef std::function<void() noexcept> work_t;

    virtual
    ~executor_t() = default;

    virtual
    auto
    spawn(work_t work) -> void = 0;

    template<class F>
    auto
    spawn(F f) -> typename std::enable_if<!std::is_copy_assignable<F>::value>::type {
        spawn(move_only_wrapper<F>(std::move(f)));
    }
};

} // namespace api
} // namespace cocaine
