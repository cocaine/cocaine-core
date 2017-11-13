#pragma once

#include "cocaine/format/base.hpp"

#include <system_error>

namespace cocaine {

template<>
struct display<std::error_code> {
    using value_type = std::error_code;

    static
    auto
    apply(std::ostream& stream, const value_type& ec) -> std::ostream& {
        return stream << cocaine::format("[{}] {}", ec.value(), ec.message());
    }
};

template<>
struct display_traits<std::error_code> : public lazy_display<std::error_code> {};

} // namespace cocaine
