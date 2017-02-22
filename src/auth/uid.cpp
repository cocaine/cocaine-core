#include "cocaine/auth/uid.hpp"

namespace cocaine {
namespace auth {

struct identity_t::data_t {
    std::vector<uid_t> uids;
};

identity_t::identity_t() :
    d(std::make_shared<data_t>())
{}

identity_t::identity_t(std::vector<uid_t> uids) :
    d(std::make_shared<data_t>())
{
    d->uids = std::move(uids);
}

auto
identity_t::uids() const -> const std::vector<uid_t>& {
    return d->uids;
}

} // namespace auth
} // namespace cocaine
