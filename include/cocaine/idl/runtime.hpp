#pragma once

#include <map>
#include <string>

#include "../dynamic.hpp"
#include "primitive.hpp"

namespace cocaine {
namespace io {

struct runtime_tag;

struct runtime {

struct metrics {
    typedef runtime_tag tag;

    constexpr static auto alias() noexcept -> const char* {
        return "metrics";
    }

    typedef option_of<
        dynamic_t
    >::tag upstream_type;
};

};

template<>
struct protocol<runtime_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        runtime::metrics
    >::type messages;
};

}  // namespace io
}  // namespace cocaine
