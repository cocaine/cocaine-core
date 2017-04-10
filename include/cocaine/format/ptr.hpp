#pragma once

#include "cocaine/format/base.hpp"

#include <memory>

namespace cocaine {

template<class T>
struct ptr_display {
    using value_type = T;
    using underlying_type = typename pristine<decltype(*std::declval<T>())>::type;

    static
    auto
    apply(std::ostream& stream, const value_type& value) -> std::ostream& {
        if(value) {
            display<underlying_type>::apply(stream, *value);
        } else {
            stream << "nullptr";
        }
        return stream;
    }
};

template<typename T>
struct display<std::shared_ptr<T>>: public ptr_display<std::shared_ptr<T>> {};

template<typename T>
struct display<std::unique_ptr<T>>: public ptr_display<std::unique_ptr<T>> {};

template<typename T>
struct display_traits<std::shared_ptr<T>>: public lazy_display<std::shared_ptr<T>> {};

template<typename T>
struct display_traits<std::unique_ptr<T>>: public lazy_display<std::unique_ptr<T>> {};

} // namespace cocaine
