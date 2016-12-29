#include "cocaine/api/auth.hpp"

namespace cocaine {
namespace auth {

class promiscuous_t : public api::auth_t {
public:
    typedef api::auth_t::callback_type callback_type;

public:
    promiscuous_t(context_t&, const std::string&, const dynamic_t&) {}

    auto
    token(callback_type callback) -> void override {
        callback({}, {});
    }

    auto
    check_permissions(const std::string&, const std::string&) const ->
        permission_t override
    {
        return allow_t();
    }
};

}   // namespace auth
}   // namespace cocaine
