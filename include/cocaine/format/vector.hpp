#pragma once

#include <vector>

#include "base.hpp"

namespace cocaine {

template<typename T>
struct display<std::vector<T>> {
    typedef std::vector<T> value_type;

    static
    auto
    apply(std::ostream& stream, const value_type& value, const std::string& delim = ", ") -> std::ostream& {
        auto written = 0;

        stream << "[";
        for (auto& v : value) {
            stream << v;
            written += 1;

            if (written > 1) {
                stream << delim;
            }
        }
        stream << "]";

        return stream;
    }

    static
    auto
    apply(const value_type& value, const std::string& delim = ", ") -> std::string {
        std::ostringstream stream;
        display<std::vector<T>>::apply(stream, value, delim);
        return stream.str();
    }
};

} // namespace cocaine
