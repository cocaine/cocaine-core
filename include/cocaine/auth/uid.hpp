#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace cocaine {
namespace auth {

using uid_t = std::uint64_t;

static constexpr uid_t superuser = std::numeric_limits<uid_t>::min();
static constexpr uid_t anonymous = std::numeric_limits<uid_t>::max();

/// Identification information of a request.
class identity_t {
    struct data_t;
    std::shared_ptr<data_t> d;

public:
    /// Constructs an anonymous identity.
    identity_t();

    /// Constructs an identity with the given UID's, which may be
    explicit identity_t(std::vector<uid_t> uids);

    /// Returns list of user identificators.
    auto
    uids() const -> const std::vector<uid_t>&;
};

} // namespace auth
} // namespace cocaine
