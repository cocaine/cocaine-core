#pragma once

#include "cocaine/format/base.hpp"
#include "cocaine/utility.hpp"

namespace cocaine {

template<typename C>
struct kv_container_display {
    typedef C value_type;

    static
    auto
    apply(std::ostream& stream, const value_type& value) -> std::ostream& {
        auto written = 0;

        stream << "{";
        for (auto& v : value) {
            if (written > 0) {
                stream << ", ";
            }

            using first_type = typename pristine<decltype(std::get<0>(v))>::type;
            using second_type = typename pristine<decltype(std::get<1>(v))>::type;
            display<first_type>::apply(stream, std::get<0>(v));
            stream << ": ";
            display<second_type>::apply(stream, std::get<1>(v));
            written += 1;
        }
        stream << "}";

        return stream;
    }
};

} // namespace cocaine
