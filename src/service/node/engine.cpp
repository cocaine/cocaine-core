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

#include "cocaine/detail/service/node/engine.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/service/node/event.hpp"
#include "cocaine/detail/service/node/manifest.hpp"
#include "cocaine/detail/service/node/profile.hpp"
#include "cocaine/detail/service/node/session.hpp"
#include "cocaine/detail/service/node/slave.hpp"
#include "cocaine/detail/service/node/stream.hpp"

#include "cocaine/detail/unique_id.hpp"

#include "cocaine/idl/node.hpp"
#include "cocaine/idl/rpc.hpp"

#include "cocaine/rpc/asio/channel.hpp"
#include "cocaine/rpc/asio/decoder.hpp"

#include "cocaine/logging.hpp"

#include "cocaine/traits/dynamic.hpp"
#include "cocaine/traits/literal.hpp"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/sum.hpp>

#include <memory>

#include <boost/filesystem/operations.hpp>

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::io;

namespace {

struct ignore {
    void
    operator()(const std::error_code& COCAINE_UNUSED_(ec)) const {
        // Do nothing.
    }
};

} // namespace

namespace {

const char* describe[] = {
    "running",
    "broken",
    "stopping",
    "stopped"
};

struct collector_t {
    template<class>
    struct result {
        typedef bool type;
    };

    template<class T>
    bool
    operator()(const T& slave) {
        const size_t load = slave.second->load();

        m_accumulator(load);

        return slave.second->active() && load;
    }

    size_t
    median() const {
        return boost::accumulators::median(m_accumulator);
    }

    size_t
    sum() const {
        return boost::accumulators::sum(m_accumulator);
    }

private:
    boost::accumulators::accumulator_set<
        size_t,
        boost::accumulators::features<
            boost::accumulators::tag::median,
            boost::accumulators::tag::sum
        >
    > m_accumulator;
};

} // namespace

namespace {

struct load {
    template<class T>
    bool
    operator()(const T& lhs, const T& rhs) const {
        return lhs.second->load() < rhs.second->load();
    }
};

struct available {
    template<class T>
    bool
    operator()(const T& slave) const {
        return slave.second->active() && slave.second->load() < max;
    }

    const size_t max;
};

template<class It, class Compare, class Predicate>
inline
It
min_element_if(It first, It last, Compare compare, Predicate predicate) {
    while(first != last && !predicate(*first)) {
        ++first;
    }

    if(first == last) {
        return last;
    }

    It result = first;

    while(++first != last) {
        if(predicate(*first) && compare(*first, *result)) {
            result = first;
        }
    }

    return result;
}

} // namespace

engine_t::engine_t(context_t& context, const manifest_t& manifest, const profile_t& profile):
    m_context(context),
    m_log(context.log(manifest.name)),
    m_manifest(manifest),
    m_profile(profile),
    m_state(states::stopped),
    m_termination_timer(m_loop),
    m_socket(m_loop),
    m_acceptor(m_loop, protocol_type::endpoint(m_manifest.endpoint)),
    m_next_id(1)
{
    m_isolate = m_context.get<api::isolate_t>(
        m_profile.isolate.type,
        m_context,
        m_manifest.name,
        m_profile.isolate.args
    );
    COCAINE_LOG_DEBUG(m_log, "app '%s' engine has been published on '%s'", m_manifest.name, m_acceptor.local_endpoint().path());
    m_thread = std::thread(std::bind(&engine_t::run, this));
}

engine_t::~engine_t() {
    COCAINE_LOG_DEBUG(m_log, "stopping '%s' engine", m_manifest.name);

    boost::filesystem::remove(m_manifest.endpoint);

    m_loop.post(std::bind(&engine_t::migrate, this, states::stopping));

    if(m_thread.joinable()) {
        m_thread.join();
    }
}

void
engine_t::run() {
    COCAINE_LOG_DEBUG(m_log, "starting the '%s' engine", m_manifest.name);

    m_acceptor.async_accept(
        m_socket,
        m_endpoint,
        std::bind(&engine_t::on_accept, this, ph::_1)
    );

    m_state = states::running;
    std::error_code ec;
    m_loop.run(ec);
    if(ec) {
        COCAINE_LOG_DEBUG(m_log, "engine has been stopped with error: [%d] %s", ec.value(), ec.message());
    } else {
        COCAINE_LOG_DEBUG(m_log, "engine has been successfuly stopped");
    }
    m_state = states::stopped;
}

std::shared_ptr<api::stream_t>
engine_t::enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream) {
    if(m_state != states::running) {
        throw cocaine::error_t("the engine is not active");
    }

    auto session = std::make_shared<session_t>(m_next_id++, event, upstream);

    std::lock_guard<session_queue_t> lock(m_queue);
    if(m_profile.queue_limit > 0 && m_queue.size() >= m_profile.queue_limit) {
        throw cocaine::error_t("the queue is full");
    }

    m_queue.push(session);
    wake();
    return std::make_shared<session_t::downstream_t>(session);
}

std::shared_ptr<api::stream_t>
engine_t::enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream, const std::string& tag) {
    if(m_state != states::running) {
        throw cocaine::error_t("the engine is not active");
    }

    auto session = std::make_shared<session_t>(m_next_id++, event, upstream);

    pool_map_t::iterator it;

    {
        std::lock_guard<std::mutex> pool_guard(m_pool_mutex);

        it = m_pool.find(tag);

        if(it == m_pool.end()) {
            if(m_pool.size() >= m_profile.pool_limit) {
                throw cocaine::error_t("the pool is full");
            }

            std::tie(it, std::ignore) = m_pool.insert(
                std::make_pair(
                    tag,
                    std::make_shared<slave_t>(
                        tag,
                        m_manifest,
                        m_profile,
                        m_context,
                        std::bind(&engine_t::wake, this),
                        std::bind(&engine_t::erase, this, ph::_1, ph::_2, ph::_3),
                        m_loop
                    )
                )
            );
        }
    }

    it->second->assign(session);

    return std::make_shared<session_t::downstream_t>(session);
}

void
engine_t::erase(const std::string& id, int code, const std::string& reason) {
    COCAINE_LOG_DEBUG(m_log, "erasing slave '%s' from the pool", id);

    std::lock_guard<std::mutex> lock(m_pool_mutex);
    m_pool.erase(id);

    if(code == rpc::terminate::abnormal) {
        COCAINE_LOG_ERROR(m_log, "the app seems to be broken: %s", reason);
        migrate(states::broken);
    }

    if(m_state != states::running && m_pool.empty()) {
        // If it was the last slave, shut the engine down.
        stop();
    } else {
        wake();
    }
}

void engine_t::wake() {
    m_loop.post(std::bind(&engine_t::do_wake, this));
}

void
engine_t::do_wake() {
    pump();
    balance();
}

// Collect info about engine's status. Must be invoked only from engine's thread.
void
engine_t::do_info(std::function<void(dynamic_t::object_t)> callback) {
    BOOST_ASSERT(std::this_thread::get_id() == m_thread.get_id());

    collector_t collector;

    std::lock_guard<std::mutex> plock(m_pool_mutex);

    size_t active = std::count_if(
        m_pool.begin(),
        m_pool.end(),
        std::bind<bool>(std::ref(collector), ph::_1)
    );

    std::lock_guard<session_queue_t> qlock(m_queue);
    dynamic_t::object_t info;
    info["profile"] = m_profile.name;
    info["load-median"] = dynamic_t::uint_t(collector.median());
    info["queue"] = dynamic_t::object_t(
        {
            { "capacity", dynamic_t::uint_t(m_profile.queue_limit) },
            { "depth",    dynamic_t::uint_t(m_queue.size()) }
        }
    );
    info["sessions"] = dynamic_t::object_t(
        {
            { "pending", dynamic_t::uint_t(collector.sum()) }
        }
    );
    info["slaves"] = dynamic_t::object_t(
        {
            { "active",   dynamic_t::uint_t(active) },
            { "capacity", dynamic_t::uint_t(m_profile.pool_limit) },
            { "idle",     dynamic_t::uint_t(m_pool.size() - active) }
        }
    );
    info["state"] = std::string(describe[static_cast<int>(m_state)]);

    callback(std::move(info));
}

void
engine_t::info(std::function<void(dynamic_t::object_t)> callback) {
    m_loop.post(std::bind(&engine_t::do_info, this, callback));
}

void
engine_t::on_accept(const std::error_code& ec) {
    if(ec) {
        if(ec == asio::error::operation_aborted) {
            return;
        }

        COCAINE_LOG_ERROR(m_log, "unable to accept '%s' worker connection: %s", m_manifest.name, ec.message());
        return;
    }

    COCAINE_LOG_INFO(m_log, "accepted new '%s' engine client", m_manifest.name);
    on_connection(std::make_unique<protocol_type::socket>(std::move(m_socket)));

    m_acceptor.async_accept(
        m_socket,
        m_endpoint,
        std::bind(&engine_t::on_accept, this, ph::_1)
    );
}

void
engine_t::on_connection(std::unique_ptr<protocol_type::socket>&& socket) {
    const int fd = socket->native_handle();

    COCAINE_LOG_DEBUG(m_log, "initiating a slave handshake from %d fd", fd);
    auto channel = std::make_shared<io::channel<protocol_type>>(std::move(socket));
    channel->reader->read(
        m_message,
        std::bind(&engine_t::on_maybe_handshake, this, ph::_1, fd)
    );
    m_backlog[fd] = channel;
}

void
engine_t::on_maybe_handshake(const std::error_code& ec, int fd) {
    if(ec) {
        on_disconnect(fd, ec);
    } else {
        on_handshake(fd, m_message);
    }
}

void
engine_t::on_handshake(int fd, const decoder_t::message_type& message) {
    COCAINE_LOG_DEBUG(m_log, "received possible handshake from slave on %d fd", fd);
    auto fit = m_backlog.find(fd);
    if(fit == m_backlog.end()) {
        COCAINE_LOG_WARNING(m_log, "disconnecting an unexpected slave on %d fd", fd);
        return;
    }

    auto channel = fit->second;
    // Pop the channel.
    m_backlog.erase(fd);

    std::string id;
    try {
        io::type_traits<
            typename io::event_traits<rpc::handshake>::argument_type
        >::unpack(message.args(), id);
    } catch(const std::exception& e) {
        COCAINE_LOG_WARNING(m_log, "disconnecting an incompatible slave on %d fd: %s", fd, e.what());
        return;
    }

    pool_map_t::iterator it;

    {
        std::lock_guard<std::mutex> lock(m_pool_mutex);
        it = m_pool.find(id);
        if(it == m_pool.end()) {
            COCAINE_LOG_WARNING(m_log, "disconnecting an unknown '%s' slave on %d fd", id, fd);
            return;
        }
    }

    COCAINE_LOG_DEBUG(m_log, "slave '%s' on %d fd connected", id, fd);
    it->second->bind(channel);
}

void
engine_t::on_disconnect(int fd, const std::error_code& ec) {
    if(ec == asio::error::operation_aborted) {
        return;
    }
    COCAINE_LOG_INFO(m_log, "slave on %d fd has disconnected during the handshake: [%d] %s", fd, ec.value(), ec.message());
    m_backlog.erase(fd);
}

void
engine_t::on_termination(const std::error_code& ec) {
    if(ec == asio::error::operation_aborted) {
        return;
    }

    std::lock_guard<session_queue_t> queue_guard(m_queue);
    COCAINE_LOG_WARNING(m_log, "forcing the engine termination due to timeout");
    stop();
}

void
engine_t::pump() {
    session_queue_t::value_type session;

    while(!m_queue.empty()) {
        std::lock_guard<std::mutex> pool_guard(m_pool_mutex);

        const auto it = min_element_if(m_pool.begin(), m_pool.end(), load(), available {
            m_profile.concurrency
        });

        if(it == m_pool.end()) {
            return;
        }

        {
            std::lock_guard<session_queue_t> lock(m_queue);
            if(m_queue.empty()) {
                return;
            }

            // Move out a new session from the queue.
            session = std::move(m_queue.front());

            // Destroy an empty session husk.
            m_queue.pop_front();
        }

        // Process the queue head outside the lock, because it might take some considerable amount
        // of time if the session has expired and there's some heavy-lifting in the error handler.
        it->second->assign(session);
    }
}

void
engine_t::balance() {
    std::lock_guard<std::mutex> pool_guard(m_pool_mutex);

    if(m_pool.size() >= m_profile.pool_limit ||
       m_pool.size() * m_profile.grow_threshold >= m_queue.size())
    {
        return;
    }

    const unsigned int target = std::min(
        m_profile.pool_limit,
        std::max(
            1UL,
            m_queue.size() / m_profile.grow_threshold
        )
    );

    if(target <= m_pool.size()) {
        return;
    }

    COCAINE_LOG_INFO(m_log, "enlarging the slaves pool from %d to %d", m_pool.size(), target);

    while(m_pool.size() != target) {
        const auto id = unique_id_t().string();
        m_pool[id] = std::make_shared<slave_t>(
            id,
            m_manifest,
            m_profile,
            m_context,
            std::bind(&engine_t::wake, this),
            std::bind(&engine_t::erase, this, ph::_1, ph::_2, ph::_3),
            m_loop
        );
    }
}

void
engine_t::migrate(states target) {
    std::lock_guard<session_queue_t> lock(m_queue);

    m_state = target;

    if(!m_queue.empty()) {
        COCAINE_LOG_DEBUG(
            m_log,
            "dropping %llu incomplete session(s) due to the engine state migration",
            m_queue.size()
        );

        // Abort all the outstanding sessions.
        while(!m_queue.empty()) {
            m_queue.front()->upstream->error(
                error::resource_error,
                "engine is shutting down"
            );

            m_queue.pop_front();
        }
    }

    unsigned int pending = 0;

    // NOTE: Send the termination event to the active slaves.
    // If there're no active slaves, the engine can terminate right away,
    // otherwise, the engine should wait for the specified timeout for slaves
    // to finish their sessions and, if they are still active, force the termination.

    for(auto it = m_pool.begin(); it != m_pool.end(); ++it) {
        if(it->second->active()) {
            it->second->stop();
            ++pending;
        }
    }

    if(!pending) {
        stop();
    } else {
        COCAINE_LOG_INFO(m_log, "waiting for %d active session(s) to terminate", pending)(
            "timeout", m_profile.termination_timeout
        );

        m_termination_timer.expires_from_now(boost::posix_time::seconds(m_profile.termination_timeout));
        m_termination_timer.async_wait(std::bind(&engine_t::on_termination, this, ph::_1));
    }
}

void
engine_t::stop() {
    COCAINE_LOG_DEBUG(m_log, "stopping '%s' engine", m_manifest.name);
    m_acceptor.cancel();
    m_termination_timer.cancel();

    // NOTE: This will force the slave pool termination.
    m_pool.clear();

    if(m_state == states::stopping) {
        m_state = states::stopped;
        // Don't stop the event loop explicitly. Instead of this - we cancel all handlers and
        // wait for graceful shutdown.
    }
}
