#include "event.hpp"

#include <future>

#include <blackhole/logger.hpp>

#include "cocaine/api/auth.hpp"
#include "cocaine/api/unicorn.hpp"
#include "cocaine/dynamic.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/unicorn/value.hpp"

namespace cocaine {
namespace controller {
namespace event {

namespace ph = std::placeholders;

class event_t::control_t : public std::enable_shared_from_this<event_t::control_t> {
    using method_t = std::string;

    using uid_t = api::uid_t;
    const std::string path;
    const std::shared_ptr<logging::logger_t> log;

    // ACL mapping. for example:
    //
    // ```
    // "unicorn": {
    //     // Read-only access for user `100500000001232300`.
    //     "100500000001232300": {
    //         "get": {
    //             "rate_limit": 10
    //         },
    //         "subscribe": {},
    //         "increment": {},
    //         "children_subscribe": {}
    //     }
    // }
    // ```
    synchronized<std::map<uid_t, std::map<method_t, dynamic_t>>> acl;

    std::shared_ptr<api::unicorn_t> unicorn;
    std::shared_ptr<api::unicorn_scope_t> observer;
    synchronized<std::unordered_map<uid_t, std::shared_ptr<api::unicorn_scope_t>>> observers;

public:
    control_t(std::string path,
              std::shared_ptr<logging::logger_t> log,
              std::shared_ptr<api::unicorn_t> unicorn) :
        path(std::move(path)),
        log(std::move(log)),
        unicorn(std::move(unicorn))
    {}

    auto
    ensure(const method_t& event, const std::vector<uid_t>& uids) -> void {
        if (uids.empty()) {
            throw std::system_error(make_error_code(error::unauthorized));
        }

        acl.apply([&](std::map<uid_t, std::map<method_t, dynamic_t>>& acl) {
            const auto allowed = std::all_of(
                std::begin(uids),
                std::end(uids),
                [&](const uid_t& uid) {
                    return acl.count(uid) > 0 && acl[uid].count(event) > 0;
                }
            );

            if (!allowed) {
                throw std::system_error(make_error_code(error::permission_denied));
            }
        });
    }

    auto
    update() -> void {
        observer = unicorn->children_subscribe(
            std::bind(&control_t::on_change, shared_from_this(), ph::_1),
            path
        );
    }

    auto
    cancel() -> void {
        observer.reset();
        observers->clear();
    }

private:
    auto
    on_change(std::future<api::unicorn_t::response::children_subscribe> result) -> void {
        try {
            std::vector<std::string> nodes;
            std::tie(std::ignore, nodes) = result.get();

            std::vector<uid_t> users;

            try {
                std::transform(
                    std::begin(nodes),
                    std::end(nodes),
                    std::back_inserter(users),
                    boost::lexical_cast<uid_t, std::string>
                );
            } catch (const boost::bad_lexical_cast&) {
                throw std::system_error(make_error_code(error::invalid_acl_framing));
            }

            COCAINE_LOG_INFO(log, "received {} ACL(s) updated", users.size());

            acl.apply([&](std::map<uid_t, std::map<method_t, dynamic_t>>& acl) {
                std::transform(
                    std::begin(users),
                    std::end(users),
                    std::inserter(acl, std::begin(acl)),
                    [](uid_t user) {
                        return std::make_pair(user, std::map<method_t, dynamic_t>());
                    }
                );
            });

            for (auto user : users) {
                auto observer = unicorn->subscribe(
                    std::bind(&control_t::on_update, shared_from_this(), user, ph::_1),
                    cocaine::format("{}/{}", path, user)
                );

                observers->insert(std::make_pair(user, observer));
            }
        } catch (const std::system_error& err) {
            // TODO: Replace magic value with meaningful Unicorn error code.
            if (err.code().value() != -101) {
                COCAINE_LOG_ERROR(log, "failed to update ACL list: {}", error::to_string(err));
            }
        }
    }

    auto
    on_update(uid_t user, std::future<api::unicorn_t::response::get> result) -> void {
        try {
            const auto events = result.get().get_value();
            COCAINE_LOG_INFO(log, "received ACL updates for user {}", user);

            acl.apply([&](std::map<uid_t, std::map<method_t, dynamic_t>>& acl) {
                if (events.convertible_to<std::map<method_t, dynamic_t>>()) {
                    acl[user] = events.to<std::map<method_t, dynamic_t>>();
                } else {
                    throw std::system_error(make_error_code(error::invalid_acl_framing));
                }
            });
        } catch (const std::system_error& err) {
            COCAINE_LOG_ERROR(log, "failed to update ACL for user {}: {}", user,
                error::to_string(err));
        }
    }
};

null_t::null_t(context_t&, const std::string&, const dynamic_t&) {}

auto
null_t::verify(const std::string&, const std::vector<uid_t>&) -> void {}

event_t::event_t(context_t& context, const std::string& service, const dynamic_t& args) :
    log(context.log(format("controller/{}/event", service)))
{
    auto unicorn = api::unicorn(context, args.as_object().at("unicorn", "core").as_string());

    control = std::make_shared<control_t>(
        cocaine::format("/acl/{}", service),
        log,
        std::move(unicorn)
    );
    control->update();
}

event_t::~event_t() {
    control->cancel();
}

auto
event_t::verify(const std::string& event,  const std::vector<uid_t>& uids) -> void {
    control->ensure(event, uids);
}

} // namespace event
} // namespace controller
} // namespace cocaine
