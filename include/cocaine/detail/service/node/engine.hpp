/*
    Copyright (c) 2011-2014 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2014 Other contributors as noted in the AUTHORS file.

    This file is part of Cocaine.

    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COCAINE_APP_ENGINE_HPP
#define COCAINE_APP_ENGINE_HPP

#include "cocaine/common.hpp"

#include "cocaine/api/isolate.hpp"

#include "cocaine/dynamic.hpp"

#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/forwards.hpp"
#include "cocaine/detail/service/node/queue.hpp"

#include "cocaine/rpc/asio/encoder.hpp"
#include "cocaine/rpc/asio/decoder.hpp"

#include <atomic>
#include <mutex>

#include <asio/deadline_timer.hpp>
#include <asio/local/stream_protocol.hpp>

namespace cocaine { namespace engine {

class slave_t;

class engine_t {
    COCAINE_DECLARE_NONCOPYABLE(engine_t)

    typedef asio::local::stream_protocol protocol_type;

    enum class states {
        running,
        broken,
        stopping,
        stopped
    };

    context_t& m_context;

    const std::unique_ptr<logging::log_t> m_log;

    // Configuration.
    const manifest_t& m_manifest;
    const profile_t& m_profile;

    // Engine state.
    states m_state;

    // Event loop.
    asio::io_service m_loop;
    asio::deadline_timer m_termination_timer;

    // Unix socket server acceptor.
    protocol_type::socket m_socket;
    protocol_type::endpoint m_endpoint;
    protocol_type::acceptor m_acceptor;

    // Session tagging.
    std::atomic<uint64_t> m_next_id;

    // Session queue.
    session_queue_t m_queue;

    // Slave pool.
    typedef std::map<
        int,
        std::shared_ptr<io::channel<protocol_type>>
    > backlog_t;

    backlog_t m_backlog;

    typedef std::map<
        std::string,
        std::shared_ptr<slave_t>
    > pool_map_t;

    pool_map_t m_pool;

    // Spawning mutex.
    std::mutex m_pool_mutex;

    // NOTE: A strong isolate reference, keeping it here
    // avoids isolate destruction, as the factory stores
    // only weak references to the isolate instances.
    api::category_traits<api::isolate_t>::ptr_type m_isolate;

    // Engine's worker thread.
    std::thread m_thread;

    // Message buffer for handshake event.
    io::decoder_t::message_type m_message;

public:
    engine_t(context_t& context, const manifest_t& manifest, const profile_t& profile);
   ~engine_t();

    // Scheduling.
    std::shared_ptr<api::stream_t>
    enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream);

    std::shared_ptr<api::stream_t>
    enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream, const std::string& tag);

    // Get information about engine's status. Fully asynchronous and thread-safe.
    void
    info(std::function<void(dynamic_t::object_t)> callback);

private:
    void
    run();

    void
    do_info(std::function<void(dynamic_t::object_t)> callback);

    // Called by acceptor, when a new connection from worker comes.
    void
    on_accept(const std::error_code& ec);

    // Called on successful connection with worker.
    void
    on_connection(std::unique_ptr<protocol_type::socket>&& m_socket);

    void
    on_maybe_handshake(const std::error_code& ec, int fd);

    void
    on_handshake(int fd, const io::decoder_t::message_type& m_message);

    void
    on_disconnect(int fd, const std::error_code& ec);

    void
    on_termination(const std::error_code& ec);

    void
    erase(const std::string& id, int code, const std::string& reason);

    void
    wake();

    void
    do_wake();

    void
    pump();

    void
    balance();

    void
    migrate(states target);

    void
    stop();
};

}} // namespace cocaine::engine

#endif
