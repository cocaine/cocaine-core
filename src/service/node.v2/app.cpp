#include "cocaine/detail/service/node.v2/app.hpp"

#include "cocaine/api/isolate.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/actor.hpp"
#include "cocaine/detail/chamber.hpp"
#undef args
#include "cocaine/detail/engine.hpp"
#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

#include <tuple>

using namespace cocaine;
using namespace cocaine::service::v2;

using namespace blackhole;

namespace ph = std::placeholders;

#include <asio/local/stream_protocol.hpp>

class unix_actor_t {
    typedef asio::local::stream_protocol protocol_type;

    class accept_action_t:
        public std::enable_shared_from_this<accept_action_t>
    {
        unix_actor_t *const   parent;
        protocol_type::socket socket;

    public:
        accept_action_t(unix_actor_t *const parent):
            parent(parent),
            socket(*parent->m_asio)
        {}

        void
        operator()() {
            parent->m_acceptor->async_accept(socket, std::bind(&accept_action_t::finalize,
                shared_from_this(),
                std::placeholders::_1
            ));
        }

    private:
        void
        finalize(const std::error_code& ec) {
            // Prepare the internal socket object for consequential operations by moving its contents to a
            // heap-allocated object, which in turn might be attached to an engine.
            auto ptr = std::make_shared<protocol_type::socket>(std::move(socket));

            switch(ec.value()) {
              case 0:
                COCAINE_LOG_DEBUG(parent->m_log, "accepted remote client on fd %d", ptr->native_handle());
                parent->m_context.engine().attach(ptr, parent->m_prototype);
                break;

              case asio::error::operation_aborted:
                return;

              default:
                COCAINE_LOG_ERROR(parent->m_log, "dropped remote client: [%d] %s", ec.value(), ec.message());
            }

            // TODO: Find out if it's always a good idea to continue accepting connections no matter what.
            // For example, destroying a socket from outside this thread will trigger weird stuff on Linux.
            operator()();
        }
    };

    context_t& m_context;

    protocol_type::endpoint endpoint;

    const std::unique_ptr<logging::log_t> m_log;
    const std::shared_ptr<asio::io_service> m_asio;

    // Initial dispatch. It's the protocol dispatch that will be initially assigned to all the new
    // sessions. In case of secure actors, this might as well be the protocol dispatch to switch to
    // after the authentication process completes successfully.
    io::dispatch_ptr_t m_prototype;

    // I/O acceptor. Actors have a separate thread to accept new connections. After a connection is
    // is accepted, it is assigned to a carefully choosen thread from the main thread pool.
    std::unique_ptr<protocol_type::acceptor> m_acceptor;

    // I/O authentication & processing.
    std::unique_ptr<io::chamber_t> m_chamber;

public:
    unix_actor_t(context_t& context, protocol_type::endpoint endpoint,
                 const std::shared_ptr<asio::io_service>& asio,
                 std::unique_ptr<io::basic_dispatch_t> prototype) :
        m_context(context),
        endpoint(std::move(endpoint)),
        m_log(context.log("core::io", { attribute::make("app", prototype->name()) })),
        m_asio(asio),
        m_prototype(std::move(prototype))
    {}

//   ~unix_actor_t();

//    // Observers

//    auto
//    endpoints() const -> std::vector<protocol_type::endpoint>;

//    bool
//    is_active() const;

//    auto
//    prototype() const -> const io::basic_dispatch_t&;

//    // Modifiers

    void
    run() {
        BOOST_ASSERT(!m_chamber);

        try {
            m_acceptor = std::make_unique<protocol_type::acceptor>(*m_asio, this->endpoint);
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(m_log, "unable to bind local endpoint for service: [%d] %s",
                e.code().value(), e.code().message());
            throw;
        }

        std::error_code ec;
        const auto endpoint = m_acceptor->local_endpoint(ec);

        COCAINE_LOG_INFO(m_log, "exposing service on local endpoint %s", endpoint);

        m_asio->post(std::bind(&accept_action_t::operator(),
            std::make_shared<accept_action_t>(this)
        ));

        // The post() above won't be executed until this thread is started.
        m_chamber = std::make_unique<io::chamber_t>(m_prototype->name(), m_asio);
    }

    void
    terminate() {
        BOOST_ASSERT(m_chamber);

        // Do not wait for the service to finish all its stuff (like timers, etc). Graceful termination
        // happens only in engine chambers, because that's where client connections are being handled.
        m_asio->stop();

        std::error_code ec;
        const auto endpoint = m_acceptor->local_endpoint(ec);

        COCAINE_LOG_INFO(m_log, "removing service from local endpoint %s", endpoint);

        // Does not block, unlike the one in execution_unit_t's destructors.
        m_chamber  = nullptr;
        m_acceptor = nullptr;

        // Be ready to restart the actor.
        m_asio->reset();
    }
};

template<class T> class deduce;

// From client to worker.
class streaming_dispatch_t:
    public dispatch<io::event_traits<io::app::enqueue>::dispatch_type>
{
public:
    explicit streaming_dispatch_t(const std::string& name):
        dispatch<io::event_traits<io::app::enqueue>::dispatch_type>(name)
    {
        typedef io::protocol<io::event_traits<io::app::enqueue>::dispatch_type>::scope protocol;

        on<protocol::chunk>(std::bind(&streaming_dispatch_t::write, this, ph::_1));
        on<protocol::error>(std::bind(&streaming_dispatch_t::error, this, ph::_1, ph::_2));
        on<protocol::choke>(std::bind(&streaming_dispatch_t::close, this));
    }

private:
    void
    write(const std::string&) {
    }

    void
    error(int, const std::string&) {
    }

    void
    close() {
    }
};

template<class Event>
class streaming_slot:
    public io::basic_slot<Event>
{
public:
    typedef Event event_type;

    typedef typename io::basic_slot<event_type>::tuple_type    tuple_type;
    typedef typename io::basic_slot<event_type>::dispatch_type dispatch_type;
    typedef typename io::basic_slot<event_type>::upstream_type upstream_type;

    typedef typename boost::function_types::function_type<
        typename mpl::copy<
            typename io::basic_slot<Event>::sequence_type,
            mpl::back_inserter<
                mpl::vector<
                    std::shared_ptr<const dispatch_type>,
                    typename std::add_lvalue_reference<upstream_type>::type
                >
            >
        >::type
    >::type function_type;

    typedef std::function<function_type> callable_type;

private:
    const callable_type fn;

public:
    streaming_slot(callable_type fn):
        fn(std::move(fn))
    {}

    virtual
    boost::optional<std::shared_ptr<const dispatch_type>>
    operator()(tuple_type&& args, upstream_type&& upstream) override {
        return cocaine::tuple::invoke(fn, std::tuple_cat(std::tie(upstream), std::move(args)));
    }
};

class app_service_t;
class overlord_t:
    public dispatch<io::rpc_tag>
{
public:
    overlord_t(app_service_t*) :
        dispatch<io::rpc_tag>("name/uuid")
    {}
};

/// App service actor owns it.
class app_service_t:
    public dispatch<io::app_tag>
{
    typedef streaming_slot<io::app::enqueue> slot_type;

    std::unique_ptr<logging::log_t> log;
    std::unique_ptr<unix_actor_t> actor;

public:
    app_service_t(context_t& context, const engine::manifest_t& manifest) :
        dispatch<io::app_tag>(manifest.name),
        log(context.log(cocaine::format("app/%s", manifest.name)))
    {
        on<io::app::enqueue>(std::make_shared<slot_type>(
            std::bind(&app_service_t::on_enqueue, this, ph::_1, ph::_2, ph::_3)
        ));

        // Create unix actor and bind to {name}.sock. Owns: 1:1.
        actor.reset(new unix_actor_t(
            context,
            manifest.endpoint,
            std::make_shared<asio::io_service>(),
            std::make_unique<overlord_t>(this)
        ));
        actor->run();
    }

    ~app_service_t() {
        actor->terminate();
    }

private:
    std::shared_ptr<const slot_type::dispatch_type>
    on_enqueue(slot_type::upstream_type&, const std::string& event, const std::string& tag) {
        if(tag.empty()) {
            COCAINE_LOG_DEBUG(log, "processing enqueue '%s' event", event);
            // Create message queue and cache `invoke` event immediately.
            // Create dispatch and pass `queue` there. This will be user -> worker channel.
            // Get client from the pool (by magic or some statistics). Create if necessary and inject session into message queue.
            // Invoke `client.invoke(dispatch, args...) -> stream`. This will be invoke + user -> worker channel.
            return std::make_shared<const streaming_dispatch_t>(name());
        } else {
            COCAINE_LOG_DEBUG(log, "processing enqueue '%s' event with tag '%s'", event, tag);
            // TODO: Complete!
            throw cocaine::error_t("on_enqueue: not implemented yet");
        }
    }
};

app_t::app_t(context_t& context, const std::string& manifest, const std::string& profile) :
    context(context),
    manifest(new engine::manifest_t(context, manifest)),
    profile(new engine::profile_t(context, profile))
{
    auto isolate = context.get<api::isolate_t>(
        this->profile->isolate.type,
        context,
        this->manifest->name,
        this->profile->isolate.args
    );

    // TODO: Start the service immediately, but set its state to `spooling` or somethinh else.
    // While in this state it can serve requests, but always return `invalid state` error.
    if(this->manifest->source() != cached<dynamic_t>::sources::cache) {
        isolate->spool();
    }

    start();
}

app_t::~app_t() {
    // TODO: Anounce all opened sessions to be closed (and sockets).
    context.remove(manifest->name);
}

void app_t::start() {
    context.insert(manifest->name, std::make_unique<actor_t>(
        context,
        std::make_shared<asio::io_service>(),
        std::make_unique<app_service_t>(context, *manifest))
    );
}
