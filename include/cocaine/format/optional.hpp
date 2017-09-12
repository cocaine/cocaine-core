#pragma once

#include "cocaine/format/base.hpp"
#include "cocaine/utility.hpp"

#include <boost/optional/optional.hpp>

namespace cocaine {

template<typename T>
struct display<boost::optional<T>> {
    using value_type = boost::optional<T>;
    using underlying_type = typename pristine<T>::type;

    static
    auto
    apply(std::ostream& stream, const value_type& value) -> std::ostream& {
        if (value) {
            display<underlying_type>::apply(stream, *value);
        } else {
            stream << "none";
        }
        return stream;
    }
};

template<typename T>
struct display_traits<boost::optional<T>> : public lazy_display<boost::optional<T>> {};

} // namespace cocaine
