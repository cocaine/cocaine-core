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
            if (written > 0) {
                stream << delim;
            }

            stream << v;
            written += 1;
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

template<typename T>
struct display_traits<std::vector<T>> {
    static
    auto
    apply(const std::vector<T>& value) -> std::function<std::ostream&(std::ostream&)> {
        return [&](std::ostream& stream) -> std::ostream& {
            return display<std::vector<T>>::apply(stream, value);
        };
    }
};

} // namespace cocaine
