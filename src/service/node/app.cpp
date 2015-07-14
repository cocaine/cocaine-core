#include "cocaine/detail/service/node/app.hpp"

#include "cocaine/api/isolate.hpp"
#include "cocaine/context.hpp"
#include "cocaine/errors.hpp"
#include "cocaine/idl/node.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/rpc/actor.hpp"
#include "cocaine/traits/dynamic.hpp"

#include "cocaine/detail/service/node/actor.hpp"
#include "cocaine/detail/service/node/balancing/load.hpp"
#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/handshaker.hpp"
#include "cocaine/detail/service/node/dispatch/hostess.hpp"
#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/overseer.hpp"
#include "cocaine/detail/service/node/profile.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::service;
using namespace cocaine::service::node;

/// App dispatch, manages incoming enqueue requests. Adds them to the queue.
class app_dispatch_t:
    public dispatch<io::app_tag>
{
    typedef io::streaming_slot<io::app::enqueue> slot_type;

    const std::unique_ptr<logging::log_t> log;

    // Yes, weak pointer here indicates about application destruction.
    std::weak_ptr<overseer_t> overseer;

public:
    app_dispatch_t(context_t& context, const std::string& name, std::shared_ptr<overseer_t> overseer_) :
        dispatch<io::app_tag>(name),
        log(context.log(format("%s/dispatch", name))),
        overseer(std::move(overseer_))
    {
        on<io::app::enqueue>(std::make_shared<slot_type>(
            std::bind(&app_dispatch_t::on_enqueue, this, ph::_1, ph::_2, ph::_3)
        ));

        on<io::app::info>(std::bind(&app_dispatch_t::on_info, this));
    }

    ~app_dispatch_t() {
        COCAINE_LOG_TRACE(log, "app dispatch has been destroyed");
    }

private:
    std::shared_ptr<const slot_type::dispatch_type>
    on_enqueue(slot_type::upstream_type&& upstream , const std::string& event, const std::string& id) {
        COCAINE_LOG_DEBUG(log, "processing enqueue '%s' event", event);

        if (auto overseer = this->overseer.lock()) {
            if (id.empty()) {
                return overseer->enqueue(std::move(upstream), event, boost::none);
            } else {
                return overseer->enqueue(std::move(upstream), event, service::node::slave::id_t(id));
            }
        } else {
            upstream.send<
                io::protocol<io::event_traits<io::app::enqueue>::dispatch_type>::scope::error
            >(std::make_error_code(std::errc::broken_pipe), std::string("the application has been stopped"));

            return nullptr;
        }
    }

    dynamic_t
    on_info() const {
        if (auto overseer = this->overseer.lock()) {
            return overseer->info();
        }

        throw std::system_error(std::make_error_code(std::errc::broken_pipe), "the application has been stopped");
    }
};

/// \note originally there was `boost::variant`, but in 1.46 it has no idea about move-only types.
namespace state {

class base_t {
public:
    virtual
    ~base_t() {}

    virtual
    bool
    stopped() noexcept {
        return false;
    }

    virtual
    dynamic_t::object_t
    info(app_t::info_policy_t policy) const = 0;
};

/// The application is stopped either normally or abnormally.
class stopped_t:
    public base_t
{
    std::string cause;

public:
    stopped_t() noexcept:
        cause("uninitialized")
    {}

    explicit
    stopped_t(std::string cause) noexcept:
        cause(std::move(cause))
    {}

    virtual
    bool
    stopped() noexcept {
        return true;
    }

    virtual
    dynamic_t::object_t
    info(app_t::info_policy_t) const {
        dynamic_t::object_t info;
        info["state"] = "stopped";
        info["cause"] = cause;
        return info;
    }
};

/// The application is currently spooling.
class spooling_t:
    public base_t
{
    std::shared_ptr<api::isolate_t> isolate;
    std::unique_ptr<api::cancellation_t> spooler;

public:
    template<class F>
    spooling_t(context_t& context, asio::io_service& loop, const manifest_t& manifest, const profile_t& profile, F cb) {
        isolate = context.get<api::isolate_t>(
            profile.isolate.type,
            context,
            loop,
            manifest.name,
            profile.isolate.args
        );

        spooler = isolate->async_spool(std::move(cb));
    }

    ~spooling_t() {
        spooler->cancel();
    }

    virtual
    dynamic_t::object_t
    info(app_t::info_policy_t) const {
        dynamic_t::object_t info;
        info["state"] = "spooling";
        return info;
    }
};

/// The application has been published and currently running.
class running_t:
    public base_t
{
    const logging::log_t* log;

    context_t& context;

    std::string name;

    std::unique_ptr<unix_actor_t> engine;
    std::shared_ptr<overseer_t> overseer;

public:
    running_t(context_t& context_,
              const manifest_t& manifest,
              const profile_t& profile,
              const logging::log_t* log,
              std::shared_ptr<asio::io_service> loop):
        log(log),
        context(context_),
        name(manifest.name)
    {
        // Create the Overseer - slave spawner/despawner plus the event queue dispatcher.
        overseer.reset(new overseer_t(context, manifest, profile, loop));

        // Create the event balancer.
        // TODO: Rename method.
        overseer->balance(std::make_unique<load_balancer_t>(overseer));

        // Create a TCP server and publish it.
        COCAINE_LOG_TRACE(log, "publishing application service with the context");
        context.insert(manifest.name, std::make_unique<actor_t>(
            context,
            std::make_shared<asio::io_service>(),
            std::make_unique<app_dispatch_t>(context, manifest.name, overseer)
        ));

        // Create an unix actor and bind to {manifest->name}.{pid} unix-socket.
        COCAINE_LOG_TRACE(log, "publishing worker service with the context");
        engine.reset(new unix_actor_t(
            context,
            manifest.endpoint,
            std::bind(&overseer_t::prototype, overseer),
            [](io::dispatch_ptr_t handshaker, std::shared_ptr<session_t> session) {
                std::static_pointer_cast<const handshaker_t>(handshaker)->bind(session);
            },
            std::make_shared<asio::io_service>(),
            std::make_unique<hostess_t>(manifest.name)
        ));
        engine->run();
    }

    ~running_t() {
        COCAINE_LOG_TRACE(log, "removing application service from the context");

        context.remove(name);

        engine->terminate();
        overseer->terminate();
    }

    virtual
    dynamic_t::object_t
    info(app_t::info_policy_t policy) const {
        dynamic_t::object_t info;

        if (policy == app_t::info_policy_t::verbose) {
            info = overseer->info();
        }

        info["state"] = "running";
        return info;
    }
};

} // namespace state

class cocaine::service::node::app_state_t:
    public std::enable_shared_from_this<app_state_t>
{
    const std::unique_ptr<logging::log_t> log;

    context_t& context;

    typedef std::unique_ptr<state::base_t> state_type;

    synchronized<state_type> state;

    /// Node start request's deferred.
    cocaine::deferred<void> deferred;

    // Configuration.
    const manifest_t manifest_;
    const profile_t  profile;

    std::shared_ptr<asio::io_service> loop;
    std::unique_ptr<asio::io_service::work> work;
    boost::thread thread;

public:
    app_state_t(context_t& context,
                manifest_t manifest_,
                profile_t profile_,
                cocaine::deferred<void> deferred_):
        log(context.log(format("%s/app", manifest_.name))),
        context(context),
        state(new state::stopped_t),
        deferred(std::move(deferred_)),
        manifest_(std::move(manifest_)),
        profile(std::move(profile_)),
        loop(std::make_shared<asio::io_service>()),
        work(std::make_unique<asio::io_service::work>(*loop))
    {
        COCAINE_LOG_TRACE(log, "application has initialized its internal state");

        thread = boost::thread([=] {
            loop->run();
        });
    }

    ~app_state_t() {
        COCAINE_LOG_TRACE(log, "application is destroying its internal state");

        work.reset();
        thread.join();

        COCAINE_LOG_TRACE(log, "application has destroyed its internal state");
    }

    const manifest_t&
    manifest() const noexcept {
        return manifest_;
    }

    dynamic_t
    info(app_t::info_policy_t policy) const {
        return (*state.synchronize())->info(policy);
    }

    void
    spool() {
        state.apply([&](state_type& state) {
            if (!state->stopped()) {
                throw std::logic_error("invalid state");
            }

            COCAINE_LOG_TRACE(log, "app is spooling");
            state.reset(new state::spooling_t(
                context,
                *loop,
                manifest(),
                profile,
                std::bind(&app_state_t::on_spool, shared_from_this(), ph::_1, ph::_2)
            ));
        });
    }

    void
    cancel(std::string cause = "manually stopped") {
        state.synchronize()->reset(new state::stopped_t(std::move(cause)));
    }

private:
    void
    on_spool(const std::error_code& ec, const std::string& what) {
        if (ec) {
            const auto reason = what.empty() ? ec.message() : what;

            COCAINE_LOG_ERROR(log, "unable to spool app - [%d] %s", ec.value(), reason);

            loop->dispatch(std::bind(&app_state_t::cancel, shared_from_this(), reason));

            // Attempt to finish node service's request.
            try {
                deferred.abort(ec, reason);
            } catch (const std::exception&) {
                // Ignore if the client has been disconnected.
            }
        } else {
            // Dispatch the completion handler to be sure it will be called in a I/O thread to
            // avoid possible deadlocks (which ones?).
            loop->dispatch(std::bind(&app_state_t::publish, shared_from_this()));
        }
    }

    void
    publish() {
        try {
            state.synchronize()->reset(
                new state::running_t(context, manifest(), profile, log.get(), loop)
            );
        } catch (const std::exception& err) {
            COCAINE_LOG_ERROR(log, "unable to publish app: %s", err.what());
            cancel(err.what());
        }

        // Attempt to finish node service's request.
        try {
            deferred.close();
        } catch (const std::exception&) {
            // Ignore if the client has been disconnected.
        }
    }
};

app_t::app_t(context_t& context,
             const std::string& manifest,
             const std::string& profile,
             cocaine::deferred<void> deferred):
    state(std::make_shared<app_state_t>(context, manifest_t(context, manifest), profile_t(context, profile), deferred))
{
    state->spool();
}

app_t::~app_t() {
    if (state) {
        state->cancel();
    }
}

std::string
app_t::name() const {
    return state->manifest().name;
}

dynamic_t
app_t::info(info_policy_t policy) const {
    return state->info(policy);
}
