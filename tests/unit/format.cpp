#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cocaine/common.hpp>
#include <cocaine/format.hpp>
#include <cocaine/format/deque.hpp>
#include <cocaine/format/graph.hpp>
#include <cocaine/format/header.hpp>
#include <cocaine/format/map.hpp>
#include <cocaine/format/optional.hpp>
#include <cocaine/format/tuple.hpp>
#include <cocaine/format/vector.hpp>
#include <cocaine/forwards.hpp>
#include <cocaine/hpack/header_definitions.hpp>
#include <cocaine/rpc/traversal.hpp>

#include "test_idl.hpp"

namespace cocaine {
namespace {

template<class T>
auto
to_string(T&& t) -> std::string {
    return string_display<typename pristine<T>::type>::apply(std::forward<T>(t));
}

TEST(display, deque) {
    std::deque<int> sample{{1, 2, 3, 4}};
    ASSERT_EQ(to_string(sample), "[1, 2, 3, 4]");
}

TEST(display, graph) {
    auto root = io::traverse<io::test_tag>();
    ASSERT_TRUE(root);
    io::graph_root_t sample = *root;
    ASSERT_EQ(to_string(sample),
              "{0: [method1, {}, {}], "
              "1: [method2, {0: [inner_method1, {}]}, {0: [inner_method1, {}]}], "
              "65532: [goaway, {}, {0: [value, {}], 1: [error, {}]}], "
              "65533: [ping, {}, {0: [value, {}], 1: [error, {}]}], "
              "65534: [settings, {}, {0: [value, {}], 1: [error, {}]}], "
              "65535: [revoke, {}, {0: [value, {}], 1: [error, {}]}]}");
}

TEST(display, header) {
    auto h = hpack::header_t::create<hpack::headers::etag<>>("azaza");
    ASSERT_EQ(to_string(h), "etag: azaza");
}

TEST(display, map) {
    auto sample = std::multimap<std::string, std::string>({{"k1", "v1"}, {"k1", "v12"}, {"k2", "v2"}});
    ASSERT_EQ(to_string(sample), "{k1: v1, k1: v12, k2: v2}");

    auto sample2 = std::map<int, size_t>({{1, 2}, {42, 42}});
    ASSERT_EQ(to_string(sample2), "{1: 2, 42: 42}");
}

TEST(display, optional) {
    auto sample = boost::make_optional(42);
    ASSERT_EQ(to_string(sample), "42");

    sample = boost::none;
    ASSERT_EQ(to_string(sample), "none");
}

TEST(display, tuple) {
    auto sample = std::make_tuple(42, "qwerty", 33.5, std::vector<int>({1, 2, 3}));
    ASSERT_EQ(to_string(sample), "[42, qwerty, 33.5, [1, 2, 3]]");
}

TEST(display, vector) {
    auto sample = std::vector<std::vector<int>>({{1, 2, 3}, {1, 2, 3}});
    ASSERT_EQ(to_string(sample), "[[1, 2, 3], [1, 2, 3]]");
}

} // namespace
} // namespace cocaine
