#include "cocaine/detail/service/node.v2/app.hpp"

#include "cocaine/api/isolate.hpp"

#include "cocaine/context.hpp"
#include "cocaine/locked_ptr.hpp"

#include "cocaine/rpc/actor.hpp"

#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

#include "cocaine/detail/service/node.v2/actor.hpp"
#include "cocaine/detail/service/node.v2/balancing/load.hpp"
#include "cocaine/detail/service/node.v2/dispatch/client.hpp"
#include "cocaine/detail/service/node.v2/dispatch/handshaker.hpp"
#include "cocaine/detail/service/node.v2/dispatch/hostess.hpp"
#include "cocaine/detail/service/node.v2/overseer.hpp"

using namespace cocaine;
using namespace cocaine::service::v2;

using namespace blackhole;

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
    app_dispatch_t(context_t& context, const std::string& name, std::shared_ptr<overseer_t> overseer) :
        dispatch<io::app_tag>(name),
        log(context.log(format("%s/dispatch", name))),
        overseer(std::move(overseer))
    {
        on<io::app::enqueue>(std::make_shared<slot_type>(
            std::bind(&app_dispatch_t::on_enqueue, this, ph::_1, ph::_2, ph::_3)
        ));
    }

    ~app_dispatch_t() {
        COCAINE_LOG_TRACE(log, "app dispatch has been destroyed");
    }

private:
    std::shared_ptr<const slot_type::dispatch_type>
    on_enqueue(slot_type::upstream_type&& upstream , const std::string& event, const std::string& id) {
        COCAINE_LOG_DEBUG(log, "processing enqueue '%s' event", event);

        if (auto overseer = this->overseer.lock()) {
            return overseer->enqueue(std::move(upstream), event, id);
        } else {
            // TODO: Assign error code instead of magic.
            const int ec = 42;
            const std::string reason("the application has been closed");

            upstream.send<
                io::protocol<io::event_traits<io::app::enqueue>::dispatch_type>::scope::error
            >(ec, reason);

            return nullptr;
        }
    }
};

/// Represents a single application. Starts TCP and UNIX servers.
app_t::app_t(context_t& context_, const std::string& manifest_, const std::string& profile_) :
    context(context_),
    log(context.log(format("%s/app", manifest_))),
    manifest(new manifest_t(context, manifest_)),
    profile(new profile_t(context, profile_)),
    loop(std::make_shared<asio::io_service>())
{
    auto isolate = context.get<api::isolate_t>(
        this->profile->isolate.type,
        context,
        this->manifest->name,
        this->profile->isolate.args
    );

    // TODO: Start the service immediately, but set its state to `spooling` or somethinh else.
    // Do not publish the service until it started.
    COCAINE_LOG_TRACE(log, "spooling");
    if(this->manifest->source() != cached<dynamic_t>::sources::cache) {
        isolate->spool();
    }

    // Create the Overseer - slaves spawner/despawner plus the event queue dispatcher.
    overseer.reset(new overseer_t(context, *manifest, *profile, loop));

    // Create the event balancer.
    overseer->balance(std::make_unique<load_balancer_t>(overseer));

    // Create a TCP server and publish it.
    COCAINE_LOG_TRACE(log, "publishing application service with the context");
    context.insert(manifest->name, std::make_unique<actor_t>(
        context,
        loop,
        std::make_unique<app_dispatch_t>(context, manifest->name, overseer)
    ));

    // Create an unix actor and bind to {manifest->name}.{int} unix-socket.
    COCAINE_LOG_TRACE(log, "publishing Worker service");
    engine.reset(new unix_actor_t(
        context,
        manifest->endpoint,
        std::bind(&overseer_t::handshaker, overseer),
        [](io::dispatch_ptr_t handshaker, std::shared_ptr<session_t> session) {
            std::static_pointer_cast<const handshaker_t>(handshaker)->bind(session);
        },
        std::make_shared<asio::io_service>(),
        std::make_unique<hostess_t>(manifest->name)
    ));
    engine->run();
}

app_t::~app_t() {
    COCAINE_LOG_TRACE(log, "removing application service from the context");

    overseer->balance();

    context.remove(manifest->name);

    engine->terminate();
}
