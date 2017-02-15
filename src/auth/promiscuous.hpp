#include "cocaine/api/auth.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/lexical_cast.hpp>

namespace cocaine {
namespace auth {

class promiscuous_t : public api::auth_t {
public:
    typedef api::auth_t::callback_type callback_type;

public:
    promiscuous_t(context_t&, const std::string&, const dynamic_t&) {}

    auto
    token(callback_type callback) -> void override {
        callback({{}, {}, {}}, {}); // Bayan.
    }

    auto
    check_permissions(const std::string&, const std::string& credentials) const ->
        permission_t override
    {
        try {
            std::vector<std::string> splitted;
            boost::split(splitted, credentials, boost::is_any_of(","));

            std::vector<std::uint64_t> uids;
            for (const auto& uid : splitted) {
                uids.push_back(boost::lexical_cast<std::uint64_t>(uid));
            }
            return allow_t{std::move(uids)};
        } catch (const std::exception&) {
            return allow_t{};
        }
    }
};

}   // namespace auth
}   // namespace cocaine
