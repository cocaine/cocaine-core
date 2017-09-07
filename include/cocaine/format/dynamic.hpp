#pragma once

#include "cocaine/format/base.hpp"
#include "cocaine/dynamic.hpp"

namespace cocaine {

template<>
struct display<dynamic_t> {
    using value_type = dynamic_t;

    static
    auto
    apply(std::ostream& stream, const value_type& value) -> std::ostream& {
        return stream << boost::lexical_cast<std::string>(value);
    }
};

template<>
struct display_traits<dynamic_t> : public lazy_display<dynamic_t> {};

} // namespace cocaine
