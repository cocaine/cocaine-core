#pragma once

#include <asio/local/stream_protocol.hpp>

#include "cocaine/forwards.hpp"
#include "cocaine/locked_ptr.hpp"
#include "cocaine/rpc/session.hpp"

namespace cocaine {

class unix_actor_t {
    typedef asio::local::stream_protocol protocol_type;

    class accept_action_t;

    context_t& m_context;

    protocol_type::endpoint endpoint;

    const std::unique_ptr<logging::logger_t> m_log;

    // Initial dispatch. It's the protocol dispatch that will be initially assigned to all the new
    // sessions. In case of secure actors, this might as well be the protocol dispatch to switch to
    // after the authentication process completes successfully.
    io::dispatch_ptr_t m_prototype;

    // I/O acceptor. Actors have a separate thread to accept new connections. After a connection is
    // is accepted, it is assigned to a least busy thread from the main thread pool. Synchronized to
    // allow concurrent observing and operations.
    synchronized<std::unique_ptr<protocol_type::acceptor>> m_acceptor;

    // Main service thread.
    std::unique_ptr<io::chamber_t> m_chamber;

    typedef std::function<io::dispatch_ptr_t()> fact_type;
    typedef std::function<void(io::dispatch_ptr_t, std::shared_ptr<session_t>)> bind_type;
    fact_type fact;
    bind_type bind;

public:
    unix_actor_t(context_t& context, protocol_type::endpoint endpoint,
                 fact_type fact,
                 bind_type bind,
                 std::unique_ptr<io::basic_dispatch_t> prototype);

   ~unix_actor_t();

    void
    run();

    void
    terminate();
};

}
