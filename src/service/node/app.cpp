#include "cocaine/detail/service/node/app.hpp"

#include "cocaine/api/isolate.hpp"

#include "cocaine/context.hpp"
#include "cocaine/locked_ptr.hpp"

#include "cocaine/rpc/actor.hpp"
#include "cocaine/traits/dynamic.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

#include "cocaine/detail/service/node/actor.hpp"
#include "cocaine/detail/service/node/balancing/load.hpp"
#include "cocaine/detail/service/node/dispatch/client.hpp"
#include "cocaine/detail/service/node/dispatch/handshaker.hpp"
#include "cocaine/detail/service/node/dispatch/hostess.hpp"
#include "cocaine/detail/service/node/overseer.hpp"

using namespace cocaine;
using namespace cocaine::service;
using namespace cocaine::service::node;

namespace ph = std::placeholders;

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

        // TODO: Temporary.
        on<io::app::test>([&](const std::string& v) {
            COCAINE_LOG_DEBUG(log, "processing test '%s' event", v);

            if (v == "0") {
                overseer.lock()->terminate();
            } else {
                std::vector<std::string> slaves;
                {
                    auto pool = overseer.lock()->get_pool();
                    for (const auto& p : *pool) {
                        slaves.push_back(p.first);
                    }
                }

                for (auto& s : slaves) {
                    overseer.lock()->despawn(s, overseer_t::despawn_policy_t::graceful);
                }
            }
        });
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
            // TODO: Assign an error code instead of magic.
            const int ec = 42;
            const std::string reason("the application has been closed");

            upstream.send<
                io::protocol<io::event_traits<io::app::enqueue>::dispatch_type>::scope::error
            >(ec, reason);

            return nullptr;
        }
    }

    dynamic_t
    on_info() const {
        if (auto overseer = this->overseer.lock()) {
            return overseer->info();
        }

        // TODO: Throw system error instead.
        throw std::runtime_error("the application has been closed");
    }
};

class cocaine::service::node::app_state_t:
    public std::enable_shared_from_this<app_state_t>
{
    const std::unique_ptr<logging::log_t> log;

    context_t& context;

    enum class state_t {
        /// The application is spooling.
        spooling,
        /// The application is running and published.
        running,
        /// The application was unable to start.
        broken,
        /// The application is stopped.
        stopped,
    };

    state_t state;

    /// Breaking reason.
    std::error_code ec;

    cocaine::deferred<void> deferred;

    // Configuration.
    const manifest_t manifest_;
    const profile_t  profile;

    std::shared_ptr<api::isolate_t> isolate;
    std::unique_ptr<api::cancellation_t> spooler;

    std::shared_ptr<asio::io_service> loop;
    std::unique_ptr<unix_actor_t> engine;
    std::shared_ptr<overseer_t> overseer;

    std::unique_ptr<asio::io_service::work> work;
    boost::thread thread;

public:
    app_state_t(context_t& context, manifest_t manifest_, profile_t profile_, cocaine::deferred<void> deferred) :
        log(context.log(format("%s/app", manifest_.name))),
        context(context),
        state(state_t::stopped),
        deferred(std::move(deferred)),
        manifest_(std::move(manifest_)),
        profile(std::move(profile_)),
        loop(std::make_shared<asio::io_service>()),
        work(std::make_unique<asio::io_service::work>(*loop))
    {
        // TODO: Temporary here, use external thread.
        thread = std::move(boost::thread([=] {
            try {
                // TODO: I can use node's I/O loop instead.
                loop->run();
            } catch (const std::system_error& err) {
                COCAINE_LOG_WARNING(log, "app loop has been unexpectedly terminated: %s", err.code().message());

                ec = err.code();
                state = state_t::broken;
            }
        }));
    }

    const manifest_t&
    manifest() const noexcept {
        return manifest_;
    }

    dynamic_t
    info() const {
        dynamic_t::object_t info;

        switch (state) {
        case state_t::stopped:
            info["status"] = "stopped";
            break;
        case state_t::spooling:
            info["status"] = "spooling";
            break;
        case state_t::running:
            info = overseer->info();
            info["status"] = "running";
            break;
        case state_t::broken:
            info["status"] = "broken";
            break;
        default:
            break;
        }

        return info;
    }

    // Don't call this method twice, just don't do it.
    void
    spool() {
        BOOST_ASSERT(state == state_t::stopped);

        isolate = context.get<api::isolate_t>(
            profile.isolate.type,
            context,
            manifest().name,
            profile.isolate.args
        );

        // Do not publish the service until it started.
        COCAINE_LOG_TRACE(log, "spooling");

        state = state_t::spooling;
        spooler = isolate->spool(std::bind(&app_state_t::on_spool, shared_from_this(), ph::_1));
    }

    void
    cancel() {
        loop->dispatch(std::bind(&app_state_t::on_cancel, shared_from_this()));
    }

    // TODO: Temporary here, use external thread.
    void
    stop() {
        COCAINE_LOG_TRACE(log, "stopping the overseer thread");
        work.reset();
        thread.join();
        COCAINE_LOG_TRACE(log, "stopping the overseer thread: done");
    }

private:
    void
    on_cancel() {
        switch (state) {
        case state_t::stopped:
            break;
        case state_t::spooling:
            spooler->cancel();
            break;
        case state_t::running:
            terminate();
            break;
        case state_t::broken:
            break;
        default:
            break;
        }
    }

    void
    on_spool(const std::error_code& ec) {
        if (ec) {
            try {
                deferred.abort(ec.value(), ec.message());
            } catch (const std::exception&) {
                // Ignore.
            }
        } else {
            loop->dispatch(std::bind(&app_state_t::publish, shared_from_this()));
        }
    }

    void
    publish() {
        // Create the Overseer - slave spawner/despawner plus the event queue dispatcher.
        overseer.reset(new overseer_t(context, manifest(), profile, loop));

        // Create the event balancer.
        // TODO: Rename method.
        overseer->balance(std::make_unique<load_balancer_t>(overseer));

        // Create a TCP server and publish it.
        COCAINE_LOG_TRACE(log, "publishing application service with the context");
        context.insert(manifest().name, std::make_unique<actor_t>(
            context,
            std::make_shared<asio::io_service>(),
            std::make_unique<app_dispatch_t>(context, manifest().name, overseer)
        ));

        // Create an unix actor and bind to {manifest->name}.{int} unix-socket.
        COCAINE_LOG_TRACE(log, "publishing worker service");
        engine.reset(new unix_actor_t(
            context,
            manifest().endpoint,
            std::bind(&overseer_t::prototype, overseer),
            [](io::dispatch_ptr_t handshaker, std::shared_ptr<session_t> session) {
                std::static_pointer_cast<const handshaker_t>(handshaker)->bind(session);
            },
            std::make_shared<asio::io_service>(),
            std::make_unique<hostess_t>(manifest().name)
        ));
        engine->run();

        state = state_t::running;

        deferred.close();
    }

    void
    terminate() {
        COCAINE_LOG_TRACE(log, "removing application service from the context");

        if (overseer) {
            engine->terminate();
            overseer->terminate();
        }
    }
};

app_t::app_t(context_t& context, const std::string& manifest, const std::string& profile, cocaine::deferred<void> deferred):
    context(context),
    state(std::make_shared<app_state_t>(context, manifest_t(context, manifest), profile_t(context, profile), deferred))
{
    state->spool();
}

app_t::~app_t() {
    if (state) {
        state->cancel();

        try {
            context.remove(name());
        } catch (...) {
            // Ignore.
        }

        state->stop();
    }
}

std::string
app_t::name() const {
    return state->manifest().name;
}

dynamic_t
app_t::info() const {
    return state->info();
}
