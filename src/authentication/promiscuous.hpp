#include "cocaine/api/authentication.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

#include "cocaine/auth/uid.hpp"

namespace cocaine {
namespace authentication {

class promiscuous_t : public api::authentication_t {
public:
    typedef api::authentication_t::callback_type callback_type;

public:
    promiscuous_t(context_t&, const std::string&, const dynamic_t&) {}

    auto
    token(callback_type callback) -> void override {
        callback({{}, {}, {}}, {}); // Bayan.
    }

    using api::authentication_t::identify;

    auto
    identify(const std::string& credentials) const -> result_type override {
        try {
            std::vector<std::string> splitted;
            boost::split(splitted, credentials, boost::is_any_of(","));

            std::vector<std::uint64_t> uids;
            for (const auto& uid : splitted) {
                uids.push_back(boost::lexical_cast<std::uint64_t>(uid));
            }
            return auth::identity_t::builder_t().uids(std::move(uids)).build();
        } catch (const std::exception&) {
            return auth::identity_t::builder_t().build();
        }
    }
};

} // namespace authentication
} // namespace cocaine
