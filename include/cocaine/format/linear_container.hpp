#pragma once

#include <sstream>

#include "cocaine/format/base.hpp"

namespace cocaine {

template<typename C>
struct linear_container_display {
    typedef C value_type;

    static
    auto
    apply(std::ostream& stream, const value_type& value) -> std::ostream& {
        auto written = 0;

        stream << "[";
        for (auto& v : value) {
            if (written > 0) {
                stream << ", ";
            }

            display<typename C::value_type>::apply(stream, v);
            written += 1;
        }
        stream << "]";

        return stream;
    }
};

} // namespace cocaine
