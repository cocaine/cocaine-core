#pragma once

#include <tuple>

#include "cocaine/format/base.hpp"
#include "cocaine/tuple.hpp"

namespace cocaine {

template<typename... Args>
struct display<std::tuple<Args...>> {
    using value_type = std::tuple<Args...>;

    struct applicator_t {
        std::ostream& stream;

        template<class T>
        auto
        operator()(const T& value) -> void {
            display<typename pristine<T>::type>::apply(stream, value);
        }

        template<class T, class... TupleArgs>
        auto
        operator()(const T& value, const TupleArgs& ... args) -> void {
            display<typename pristine<T>::type>::apply(stream, value);
            stream << ", ";
            operator()(args...);
        }
    };

    static
    auto
    apply(std::ostream& stream, const value_type& value) -> std::ostream& {
        stream << "[";
        applicator_t applicator{stream};
        tuple::invoke(value, applicator);
        stream << "]";
        return stream;
    }
};

template<typename... Args>
struct display_traits<std::tuple<Args...>> : public lazy_display<std::tuple<Args...>> {};

} // namespace cocaine
