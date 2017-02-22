#include "cocaine/api/authorization/unicorn.hpp"

#include "cocaine/forwards.hpp"

namespace cocaine {
namespace authorization {
namespace unicorn {

class disabled_t : public api::authorization::unicorn_t {
public:
    disabled_t(context_t& context, const std::string& service, const dynamic_t& args);

    auto
    verify(std::size_t event, const std::string& path, const auth::identity_t& identity, callback_type callback)
        -> void override;
};

} // namespace unicorn
} // namespace authorization
} // namespace cocaine
