#pragma once

namespace cocaine {
namespace utility {

/// Replaces the value of curr with next and returns the old value of curr.
///
/// \param curr -   object whose value to replace.
/// \param next -   the value to assign to curr.
/// \return The old value of curr.
template<typename T, typename U = T>
T
exchange(T& curr, U&& next) {
    T prev = std::move(curr);
    curr = std::forward<U>(next);
    return prev;
}

}  // namespace utility
}  // namespace cocaine
