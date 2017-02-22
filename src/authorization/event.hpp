#include "cocaine/api/authorization/event.hpp"

namespace cocaine {
namespace authorization {
namespace event {

class disabled_t : public api::authorization::event_t {
public:
    disabled_t(context_t& context, const std::string& service, const dynamic_t& args);

    auto
    verify(const std::string& event, const auth::identity_t& identity) -> void override;
};

class enabled_t : public api::authorization::event_t {
    class control_t;
    std::shared_ptr<control_t> control;

    const std::shared_ptr<logging::logger_t> log;

public:
    enabled_t(context_t& context, const std::string& service, const dynamic_t& args);
   ~enabled_t();

    auto
    verify(const std::string& event, const auth::identity_t& identity) -> void override;
};

} // namespace event
} // namespace authorization
} // namespace cocaine
