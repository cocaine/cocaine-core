#include "unicorn.hpp"

namespace cocaine {
namespace authorization {
namespace unicorn {

disabled_t::disabled_t(context_t&, const std::string&, const dynamic_t&) {}

auto
disabled_t::verify(std::size_t, const std::string&, const auth::identity_t&, callback_type callback)
    -> void
{
    callback({});
}

} // namespace unicorn
} // namespace authorization
} // namespace cocaine
