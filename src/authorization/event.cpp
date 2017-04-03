#include "event.hpp"

#include <future>

#include <boost/algorithm/cxx11/all_of.hpp>

#include <blackhole/logger.hpp>

#include "cocaine/api/unicorn.hpp"
#include "cocaine/auth/uid.hpp"
#include "cocaine/context.hpp"
#include "cocaine/dynamic.hpp"
#include "cocaine/errors.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/unicorn/value.hpp"

namespace cocaine {
namespace authorization {
namespace event {

namespace ph = std::placeholders;

namespace {

const auto unicorn_no_node = -101;

const std::string prefix_acls = "/.acls";

using method_t = std::string;

struct user_t {
    using value_type = auth::uid_t;

    value_type v;

    static
    auto
    scope() -> const char* {
        return "uids";
    }

    friend
    auto
    operator<<(std::ostream& stream, const user_t& value) -> std::ostream& {
        return stream << "user " << value.v;
    }
};

struct service_t {
    using value_type = auth::cid_t;

    value_type v;

    static
    auto
    scope() -> const char* {
        return "cids";
    }

    friend
    auto
    operator<<(std::ostream& stream, const service_t& value) -> std::ostream& {
        return stream << "client " << value.v;
    }
};

} // namespace

template<typename T>
class watcher : public std::enable_shared_from_this<watcher<T>> {
    using subject_type = typename T::value_type;

    const std::string path;
    const std::shared_ptr<logging::logger_t> log;

    // ACL mapping, for example:
    //
    // ```
    // "unicorn": {
    //     "uids": {
    //         // Read-only access for user `100500000001232300`.
    //         "100500000001232300": {
    //             "get": {
    //                 "rate_limit": 10
    //             },
    //             "subscribe": {},
    //             "increment": {},
    //             "children_subscribe": {}
    //         }
    //     },
    //     "cids": {}
    // }
    // ```
    synchronized<std::map<subject_type, std::map<method_t, dynamic_t>>> acl;

    std::shared_ptr<api::unicorn_t> unicorn;
    std::shared_ptr<api::unicorn_scope_t> observer;
    synchronized<std::unordered_map<subject_type, std::shared_ptr<api::unicorn_scope_t>>> observers;

public:
    watcher(std::string path,
            std::shared_ptr<logging::logger_t> log,
            std::shared_ptr<api::unicorn_t> unicorn) :
        path(std::move(path)),
        log(std::move(log)),
        unicorn(std::move(unicorn))
    {}

    auto
    allowed_for(const std::vector<subject_type>& subjects, const method_t& event) -> bool {
        return subjects.size() > 0 && acl.apply([&](std::map<subject_type, std::map<method_t, dynamic_t>>& acl) {
            return boost::algorithm::all_of(subjects, [&](const subject_type& subject) {
                return acl.count(subject) > 0 && acl[subject].count(event) > 0;
            });
        });
    }

    auto
    update() -> void {
        observer = unicorn->children_subscribe(
            std::bind(&watcher::on_change, this->shared_from_this(), ph::_1),
            cocaine::format("{}/{}", path, T::scope())
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

            std::vector<subject_type> subjects;

            try {
                std::transform(
                    std::begin(nodes),
                    std::end(nodes),
                    std::back_inserter(subjects),
                    boost::lexical_cast<subject_type, std::string>
                );
            } catch (const boost::bad_lexical_cast&) {
                throw std::system_error(make_error_code(error::invalid_acl_framing));
            }

            COCAINE_LOG_INFO(log, "received ACL updates for {} subject(s)", subjects.size());

            acl.apply([&](std::map<subject_type, std::map<method_t, dynamic_t>>& acl) {
                std::transform(
                    std::begin(subjects),
                    std::end(subjects),
                    std::inserter(acl, std::begin(acl)),
                    [](subject_type subject) {
                        return std::make_pair(subject, std::map<method_t, dynamic_t>());
                    }
                );
            });

            for (auto subject : subjects) {
                auto observer = unicorn->subscribe(
                    std::bind(&watcher::on_update, this->shared_from_this(), T{subject}, ph::_1),
                    cocaine::format("{}/{}/{}", path, T::scope(), subject)
                );

                observers->insert(std::make_pair(subject, observer));
            }
        } catch (const std::system_error& err) {
            if (err.code().value() != unicorn_no_node) {
                COCAINE_LOG_ERROR(log, "failed to update ACL list: {}", error::to_string(err));
            }
        }
    }

    auto
    on_update(T id, std::future<api::unicorn_t::response::get> result) -> void{
        try {
            const auto events = result.get().value();
            COCAINE_LOG_INFO(log, "received ACL updates for {}", id);

            acl.apply([&](std::map<subject_type, std::map<method_t, dynamic_t>>& acl) {
                if (events.convertible_to<std::map<method_t, dynamic_t>>()) {
                    acl[id.v] = events.to<std::map<method_t, dynamic_t>>();
                } else {
                    throw std::system_error(make_error_code(error::invalid_acl_framing));
                }
            });
        } catch (const std::system_error& err) {
            if (err.code().value() == unicorn_no_node) {
                acl->erase(id.v);
                COCAINE_LOG_INFO(log, "removed ACL record for {}", id);
            } else {
                COCAINE_LOG_ERROR(log, "failed to update ACL for {}: {}", id, error::to_string(err));
            }
        }
    }
};

class enabled_t::control_t {
    struct {
        std::shared_ptr<watcher<user_t>> uid;
        std::shared_ptr<watcher<service_t>> cid;
    } watchers;

public:
    control_t(std::string path,
              std::shared_ptr<logging::logger_t> log,
              std::shared_ptr<api::unicorn_t> unicorn)
    {
        watchers.uid = std::make_shared<watcher<user_t>>(path, log, unicorn);
        watchers.cid = std::make_shared<watcher<service_t>>(path, log, unicorn);
    }

    auto
    ensure(const method_t& event, const auth::identity_t& identity) -> void {
        auto& cids = identity.cids();
        auto& uids = identity.uids();

        if (cids.empty() && uids.empty()) {
            throw std::system_error(make_error_code(error::unauthorized));
        }

        if (watchers.cid->allowed_for(cids, event) || watchers.uid->allowed_for(uids, event)) {
            return;
        } else {
            throw std::system_error(make_error_code(error::permission_denied));
        }
    }

    auto
    update() -> void {
        watchers.cid->update();
        watchers.uid->update();
    }

    auto
    cancel() -> void {
        watchers.cid->cancel();
        watchers.uid->cancel();
    }
};

disabled_t::disabled_t(context_t&, const std::string&, const dynamic_t&) {}

auto
disabled_t::verify(const std::string&, const auth::identity_t&) -> void {}

enabled_t::enabled_t(context_t& context, const std::string& service, const dynamic_t& args) :
    log(context.log(format("authorization/{}/event", service)))
{
    auto unicorn = api::unicorn(context, args.as_object().at("unicorn", "core").as_string());

    control = std::make_shared<control_t>(
        cocaine::format("{}/event/{}", prefix_acls, service),
        log,
        std::move(unicorn)
    );
    control->update();
}

enabled_t::~enabled_t() {
    control->cancel();
}

auto
enabled_t::verify(const std::string& event, const auth::identity_t& identity) -> void {
    control->ensure(event, identity);
}

} // namespace event
} // namespace authorization
} // namespace cocaine
