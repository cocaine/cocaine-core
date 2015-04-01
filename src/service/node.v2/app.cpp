#include "cocaine/detail/service/node.v2/app.hpp"

#include "cocaine/api/isolate.hpp"

#include "cocaine/context.hpp"

#include "cocaine/idl/node.hpp"

#include "cocaine/detail/actor.hpp"
#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"

#include "cocaine/idl/rpc.hpp"
#include "cocaine/idl/streaming.hpp"

#include <tuple>

using namespace cocaine;
using namespace cocaine::service::v2;

namespace ph = std::placeholders;

//class engine_t {
//    // create new session.
//    auto invoke(name, upstream) -> downstream ;
//};

#include <asio/local/stream_protocol.hpp>

//class unix_actor_t {
//    typedef asio::local::stream_protocol protocol_type;

//    class accept_action_t;

//    context_t& m_context;

//    const std::unique_ptr<logging::log_t> m_log;
//    const std::shared_ptr<asio::io_service> m_asio;

//    // Initial dispatch. It's the protocol dispatch that will be initially assigned to all the new
//    // sessions. In case of secure actors, this might as well be the protocol dispatch to switch to
//    // after the authentication process completes successfully.
//    io::dispatch_ptr_t m_prototype;

//    // I/O acceptor. Actors have a separate thread to accept new connections. After a connection is
//    // is accepted, it is assigned to a carefully choosen thread from the main thread pool.
//    std::unique_ptr<protocol_type::acceptor> m_acceptor;

//    // I/O authentication & processing.
//    std::unique_ptr<io::chamber_t> m_chamber;

//public:
//    unix_actor_t(context_t& context, const std::shared_ptr<asio::io_service>& asio,
//                 std::unique_ptr<io::basic_dispatch_t> prototype) :
//        m_context(context),
//        m_log(context.log("core::io", { attribute::make("service", prototype->name()) })),
//        m_asio(asio),
//        m_prototype(std::move(prototype))
//    {}

//    unix_actor_t(context_t& context, const std::shared_ptr<asio::io_service>& asio,
//                 std::unique_ptr<api::service_t> service);

//   ~unix_actor_t();

//    // Observers

//    auto
//    endpoints() const -> std::vector<protocol_type::endpoint>;

//    bool
//    is_active() const;

//    auto
//    prototype() const -> const io::basic_dispatch_t&;

//    // Modifiers

//    void
//    run();

//    void
//    terminate();
//};

template<class T> class deduce;

// From client to worker.
class streaming_service_t:
    public dispatch<io::event_traits<io::app::enqueue>::dispatch_type>
{
public:
    explicit streaming_service_t(const std::string& name):
        dispatch<io::event_traits<io::app::enqueue>::dispatch_type>(name)
    {
        typedef io::protocol<io::event_traits<io::app::enqueue>::dispatch_type>::scope protocol;

        on<protocol::chunk>(std::bind(&streaming_service_t::write, this, ph::_1));
        on<protocol::error>(std::bind(&streaming_service_t::error, this, ph::_1, ph::_2));
        on<protocol::choke>(std::bind(&streaming_service_t::close, this));
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
        return tuple::invoke(fn, std::tuple_cat(std::tie(upstream), std::move(args)));
    }
};

class app_service_t:
    public dispatch<io::app_tag>
{
    typedef streaming_slot<io::app::enqueue> slot_type;

    std::unique_ptr<logging::log_t> log;

public:
    app_service_t(context_t& context, const std::string& name) :
        dispatch<io::app_tag>(name),
        log(context.log(cocaine::format("app/%s", name)))
    {
        on<io::app::enqueue>(std::make_shared<slot_type>(
            std::bind(&app_service_t::on_enqueue, this, ph::_1, ph::_2, ph::_3)
        ));
    }

    std::shared_ptr<const slot_type::dispatch_type>
    on_enqueue(slot_type::upstream_type&, const std::string& event, const std::string& tag) {
        /// If pool.get() == none => spawn and put event in the queue.
        /// Else put in the client.

        if(tag.empty()) {
            COCAINE_LOG_DEBUG(log, "processing enqueue '%s' event", event);
//            downstream = enqueue(api::event_t(event), std::make_shared<engine_stream_adapter_t>(upstream));
        } else {
            COCAINE_LOG_DEBUG(log, "processing enqueue '%s' event with tag '%s'", event, tag);
            // TODO: Complete!
            throw cocaine::error_t("on_enqueue: not implemented yet");
        }

        return std::make_shared<const streaming_service_t>(name());
    }
};

app_t::app_t(context_t& context, const std::string& manifest_, const std::string& profile_) :
    context(context),
    manifest(new engine::manifest_t(context, manifest_)),
    profile(new engine::profile_t(context, profile_))
{
    auto isolate = context.get<api::isolate_t>(
        profile->isolate.type,
        context,
        manifest->name,
        profile->isolate.args
    );

    // TODO: Start the service immediately, but set its state to `spooling` or somethinh else.
    // While in this state it can serve requests, but always return `invalid state` error.
    if(manifest->source() != cached<dynamic_t>::sources::cache) {
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
        std::make_unique<app_service_t>(context, manifest->name))
    );
}
