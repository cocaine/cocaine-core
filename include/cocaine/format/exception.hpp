#pragma once

#include "cocaine/format/base.hpp"

#include <exception>

namespace cocaine {

template<>
struct display<std::exception> {
    using value_type = std::exception;

    static
    auto
    apply(std::ostream& stream, const value_type& value) -> std::ostream& {
        const std::system_error* ptr = dynamic_cast<const std::system_error*>(&value);
        if(ptr) {
            return stream << cocaine::format("[{}] {}", ptr->code().value(), ptr->what());
        } else {
            return stream << value.what();
        }
    }
};

template<>
struct display_traits<std::exception> : public lazy_display<std::exception> {};

template<>
struct display_traits<std::system_error> : public lazy_display<std::exception> {};

} // namespace cocaine
