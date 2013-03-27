/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_ENGINE_HPP
#define COCAINE_ENGINE_HPP

#include "cocaine/common.hpp"
#include "cocaine/api/isolate.hpp"
#include "cocaine/asio/reactor.hpp"
#include "cocaine/detail/atomic.hpp"
#include "cocaine/json.hpp"

#include <deque>
#include <mutex>
#include <set>

#include <boost/mpl/list.hpp>

namespace cocaine {

struct unique_id_t;

namespace io {

struct control_tag;

namespace control {
    struct report {
        typedef control_tag tag;
    };

    struct info {
        typedef control_tag tag;

        typedef boost::mpl::list<
            /* info */ Json::Value
        > tuple_type;
    };

    struct terminate {
        typedef control_tag tag;
    };
}

template<>
struct protocol<control_tag> {
    typedef boost::mpl::list<
        control::report,
        control::info,
        control::terminate
    >::type type;
};

} // namespace io

namespace engine {

struct session_t;

struct session_queue_t:
    public std::deque<std::shared_ptr<session_t>>
{
    void
    push(const_reference session);

    // Lockable concept implementation

    void
    lock() {
        m_mutex.lock();
    }

    void
    unlock() {
        m_mutex.unlock();
    }

private:
    std::mutex m_mutex;
};

class slave_t;

class engine_t:
    boost::noncopyable
{
    enum class states: int {
        running,
        broken,
        stopping,
        stopped
    };

    public:
        engine_t(context_t& context,
                 std::unique_ptr<io::reactor_t>&& reactor,
                 const manifest_t& manifest,
                 const profile_t& profile,
                 const std::shared_ptr<io::socket<io::local>>& control);

       ~engine_t();

        void
        run();

        void
        wake();

        // Scheduling

        std::shared_ptr<api::stream_t>
        enqueue(const api::event_t& event,
                const std::shared_ptr<api::stream_t>& upstream);

        void
        erase(const unique_id_t& uuid,
              int code,
              const std::string& reason);

    private:
        void
        on_connection(const std::shared_ptr<io::socket<io::local>>& socket);

        void
        on_handshake(const std::shared_ptr<io::channel<io::socket<io::local>>>& channel,
                     const io::message_t& message);

        void
        on_disconnect(const std::shared_ptr<io::channel<io::socket<io::local>>>& channel,
                      const std::error_code& ec);

        void
        on_control(const io::message_t& message);

        void
        on_cleanup(ev::timer&, int);

        void
        on_notification(ev::async&, int);

        void
        on_termination(ev::timer&, int);

        void
        pump();

        void
        balance();

        void
        migrate(states target);

        void
        stop();

    private:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;

        // Configuration

        const manifest_t& m_manifest;
        const profile_t& m_profile;

        // Engine state

        states m_state;

        // Event loop

        std::unique_ptr<io::reactor_t> m_reactor;

        ev::timer m_gc_timer,
                  m_termination_timer;

        ev::async m_notification;

        // I/O

        std::unique_ptr<io::connector<io::acceptor<io::local>>> m_connector;
        std::unique_ptr<io::channel<io::socket<io::local>>> m_channel;

        // Session queue

        std::atomic<uint64_t> m_next_id;
        session_queue_t m_queue;

        // Slave pool

        std::set<
            std::shared_ptr<io::channel<io::socket<io::local>>>
        > m_backlog;

#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            unique_id_t,
            std::shared_ptr<slave_t>
        > pool_map_t;

        pool_map_t m_pool;

        // NOTE: A strong isolate reference, keeping it here
        // avoids isolate destruction, as the factory stores
        // only weak references to the isolate instances.
        api::category_traits<api::isolate_t>::ptr_type m_isolate;
};

} // namespace engine

} // namespace cocaine

#endif
