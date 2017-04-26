#include "cocaine/rpc/actor_unix.hpp"

#include "cocaine/context.hpp"
#include "cocaine/engine.hpp"
#include "cocaine/errors.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"
#include "cocaine/rpc/basic_dispatch.hpp"

#include <blackhole/attribute.hpp>
#include <blackhole/logger.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>

#include "chamber.hpp"

using namespace cocaine;

namespace ph = std::placeholders;

class unix_actor_t::accept_action_t:
    public std::enable_shared_from_this<accept_action_t>
{
    unix_actor_t& parent;
    protocol_type::socket socket;

public:
    accept_action_t(unix_actor_t& parent, asio::io_service& loop):
        parent(parent),
        socket(loop)
    {}

    void
    operator()() {
        parent.m_acceptor.apply([this](std::unique_ptr<protocol_type::acceptor>& ptr) {
            if(!ptr) {
                COCAINE_LOG_ERROR(parent.m_log, "abnormal termination of actor connection pump");
                return;
            }

            using namespace std::placeholders;

            ptr->async_accept(socket, std::bind(&accept_action_t::finalize, shared_from_this(), ph::_1));
        });
    }

private:
    void
    finalize(const std::error_code& ec) {
        // Prepare the internal socket object for consequential operations by moving its contents to a
        // heap-allocated object, which in turn might be attached to an engine.
        auto ptr = std::make_unique<protocol_type::socket>(std::move(socket));

        switch(ec.value()) {
        case 0:
            COCAINE_LOG_DEBUG(parent.m_log, "accepted connection on fd {}", ptr->native_handle());

            try {
                auto base = parent.fact();
                auto session = parent.m_context.engine().attach(std::move(ptr), base);
                parent.bind(base, std::move(session));
            } catch(const std::system_error& e) {
                COCAINE_LOG_ERROR(parent.m_log, "unable to attach connection to engine: {}",
                    error::to_string(e));
                ptr = nullptr;
            }

            break;

        case asio::error::operation_aborted:
            return;

        default:
            COCAINE_LOG_ERROR(parent.m_log, "unable to accept connection: [{}] {}", ec.value(),
                ec.message());
            break;
        }

        // TODO: Find out if it's always a good idea to continue accepting connections no matter what.
        // For example, destroying a socket from outside this thread will trigger weird stuff on Linux.
        operator()();
    }
};

unix_actor_t::unix_actor_t(cocaine::context_t& context,
                           asio::local::stream_protocol::endpoint endpoint,
                           fact_type fact,
                           bind_type bind,
                           std::unique_ptr<cocaine::io::basic_dispatch_t> prototype) :
    m_context(context),
    endpoint(std::move(endpoint)),
    m_log(context.log("core/asio", {{ "service", prototype->name() }})),
    m_prototype(std::move(prototype)),
    fact(std::move(fact)),
    bind(bind)
{}

unix_actor_t::~unix_actor_t() = default;

void
unix_actor_t::run() {
    m_acceptor.apply([this](std::unique_ptr<protocol_type::acceptor>& ptr) {
        try {
            ptr = m_context.expose(this->endpoint);
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(m_log, "unable to bind local endpoint for service: {}",
                error::to_string(e));
            throw;
        }

        std::error_code ec;
        const auto endpoint = ptr->local_endpoint(ec);

        COCAINE_LOG_INFO(m_log, "exposing service on local endpoint {}", endpoint);

        auto action = std::make_shared<accept_action_t>(*this, ptr->get_io_service());
        ptr->get_io_service().post([=] {
            action->operator()();
        });
    });
}

void
unix_actor_t::terminate() {
    m_acceptor.apply([this](std::unique_ptr<protocol_type::acceptor>& ptr) {
        std::error_code ec;
        const auto endpoint = ptr->local_endpoint(ec);

        COCAINE_LOG_INFO(m_log, "removing service from local endpoint {}", endpoint);

        ptr = nullptr;
    });

    // TODO: Make an abstraction like `m_context.mapper().retain(endpoint);`.
    const auto endpoint = boost::lexical_cast<std::string>(this->endpoint);

    try {
        COCAINE_LOG_DEBUG(m_log, "removing local endpoint '{}'", endpoint);
        boost::filesystem::remove(endpoint);
    } catch (const std::exception& err) {
        COCAINE_LOG_WARNING(m_log, "unable to clean local endpoint '{}': {}", endpoint, err.what());
    }
}
