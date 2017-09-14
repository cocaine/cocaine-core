#pragma once

#include <vector>

#include "cocaine/format/base.hpp"
#include "cocaine/format/linear_container.hpp"

namespace cocaine {

template<typename T>
struct display<std::set<T>> : public linear_container_display<std::set<T>> {};

template<typename T>
struct display_traits<std::set<T>> : public lazy_display<std::set<T>> {};

} // namespace cocaine
