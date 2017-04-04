#pragma once

#include <ostream>
#include <sstream>

namespace cocaine {

template<typename T>
struct display {
    static
    auto
    apply(std::ostream& stream, const T& value) -> std::ostream& {
        return stream << value;
    }
};

/// Base class that can be used by display_traits to inherit from and provide lazy formatting
template<typename T>
struct lazy_display {
    struct functor_t {
        auto
        operator()(std::ostream& stream) const -> std::ostream& {
            return display<T>::apply(stream, value);
        }
        const T& value;
    };

    static
    auto
    apply(const T& value) -> functor_t {
        return functor_t{value};
    }
};

template<typename T>
struct string_display {
    static
    auto
    apply(const T& value) -> std::string {
        std::ostringstream stream;
        display<T>::apply(stream, value);
        return stream.str();
    }
};

} // namespace cocaine
