#pragma once

#include <deque>

#include "cocaine/format/base.hpp"
#include "cocaine/format/linear_container.hpp"

namespace cocaine {

template<typename T>
struct display<std::deque<T>> : public linear_container_display<std::deque<T>> {};

template<typename T>
struct display_traits<std::deque<T>> : public lazy_display<std::deque<T>> {};

} // namespace cocaine
