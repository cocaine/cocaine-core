#pragma once

#include <asio/local/stream_protocol.hpp>

#include "cocaine/forwards.hpp"

namespace cocaine {

class unix_actor_t {
    typedef asio::local::stream_protocol protocol_type;

    class accept_action_t;

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
                 std::unique_ptr<io::basic_dispatch_t> prototype);

   ~unix_actor_t();

    void
    run();

    void
    terminate();
};

}
