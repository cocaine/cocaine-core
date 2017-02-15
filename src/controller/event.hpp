#include "cocaine/api/controller.hpp"

namespace cocaine {
namespace controller {
namespace event {

using uid_t = api::uid_t;

class null_t : public api::controller::event_t {
public:
    null_t(context_t& context, const std::string& service, const dynamic_t& args);

    auto
    verify(const std::string& event, const std::vector<uid_t>& uids) -> void override;
};

class event_t : public api::controller::event_t {
    class control_t;
    std::shared_ptr<control_t> control;

    const std::shared_ptr<logging::logger_t> log;

public:
    event_t(context_t& context, const std::string& service, const dynamic_t& args);
   ~event_t();

    auto
    verify(const std::string& event, const std::vector<uid_t>& uids) -> void override;
};

} // namespace event
} // namespace controller
} // namespace cocaine
