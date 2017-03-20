#include "cocaine/auth/uid.hpp"

namespace cocaine {
namespace auth {

struct identity_t::data_t {
    std::vector<cid_t> cids;
    std::vector<uid_t> uids;
};

identity_t::builder_t::builder_t() :
    d(new data_t)
{}

identity_t::builder_t::~builder_t() = default;

auto
identity_t::builder_t::uids(std::vector<uid_t> uids) && -> builder_t&& {
    d->uids = std::move(uids);
    return std::move(*this);
}

auto
identity_t::builder_t::build() && -> identity_t {
    return identity_t{std::move(this->d)};
}

identity_t::identity_t() :
    d(std::make_shared<data_t>())
{}

identity_t::identity_t(std::unique_ptr<data_t> d) :
    d(std::move(d))
{}

identity_t::identity_t(std::vector<uid_t> uids) :
    d(std::make_shared<data_t>())
{
    d->uids = std::move(uids);
}

auto
identity_t::cids() const -> const std::vector<cid_t>& {
    return d->cids;
}

auto
identity_t::uids() const -> const std::vector<uid_t>& {
    return d->uids;
}

} // namespace auth
} // namespace cocaine
