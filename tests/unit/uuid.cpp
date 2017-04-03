#include <gtest/gtest.h>

#include <cocaine/unique_id.hpp>

namespace cocaine {
namespace {

TEST(uuid_t, operator_eq) {
    auto value = "550e8400-e29b-41d4-a716-446655440000";

    unique_id_t uuid(value);
    unique_id_t other(value);

    EXPECT_EQ(other, uuid);
}

} // namespace
} // namespace cocaine
