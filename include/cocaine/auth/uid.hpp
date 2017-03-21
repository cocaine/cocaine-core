#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace cocaine {
namespace auth {

using cid_t = std::uint32_t;
using uid_t = std::uint64_t;

static constexpr uid_t superuser = std::numeric_limits<uid_t>::min();
static constexpr uid_t anonymous = std::numeric_limits<uid_t>::max();

/// Identification information of a request.
class identity_t {
    struct data_t;
    std::shared_ptr<data_t> d;

public:
    class builder_t {
        std::unique_ptr<data_t> d;

    public:
        builder_t();
        ~builder_t();

        auto
        cids(std::vector<cid_t> cids) & -> builder_t&;

        auto
        cids(std::vector<cid_t> cids) && -> builder_t&&;

        auto
        uids(std::vector<uid_t> uids) & -> builder_t&;

        auto
        uids(std::vector<uid_t> uids) && -> builder_t&&;

        auto
        build() && -> identity_t;
    };

public:
    /// Constructs an anonymous identity.
    identity_t();

    /// Constructs an identity using private data.
    explicit identity_t(std::unique_ptr<data_t> d);

    /// Constructs an identity with the given UID's.
    __attribute__((deprecated("use `builder_t` instead")))
    explicit identity_t(std::vector<uid_t> uids);

    /// Returns the client ids.
    auto
    cids() const -> const std::vector<cid_t>&;

    /// Returns list of user identificators.
    auto
    uids() const -> const std::vector<uid_t>&;
};

} // namespace auth
} // namespace cocaine
