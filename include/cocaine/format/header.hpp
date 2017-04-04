#pragma once

#include "cocaine/format/base.hpp"
#include "cocaine/hpack/header.hpp"

namespace cocaine {

template<>
struct display<hpack::header_t> {
    using value_type = hpack::header_t;

    static
    auto
    apply(std::ostream& stream, const value_type& value) -> std::ostream& {
        return stream << value.name() << ": " << value.value();
    }
};

template<>
struct display_traits<hpack::header_t> : public string_display<hpack::header_t> {};

} // namespace cocaine
