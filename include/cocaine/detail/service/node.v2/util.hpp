#pragma once

#include <type_traits>

namespace cocaine {

template<class F>
struct move_wrapper : public F {
    move_wrapper(F&& f) : F(std::move(f)) {}

    move_wrapper(move_wrapper&&) = default;
    move_wrapper& operator=(move_wrapper&&) = default;

    /// The wrapper declares a copy constructor, tricking asio's machinery into submission,
    /// but never defines it, so that copying would result in a linking error.
    move_wrapper(const move_wrapper&);
    move_wrapper& operator=(const move_wrapper&);
};

template<class T>
auto move_handler(T&& t) -> move_wrapper<typename std::decay<T>::type> {
    return std::move(t);
}

}
