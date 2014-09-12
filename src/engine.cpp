/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/detail/engine.hpp"

#include "cocaine/api/event.hpp"
#include "cocaine/api/stream.hpp"

#include "cocaine/asio/acceptor.hpp"
#include "cocaine/asio/connector.hpp"
#include "cocaine/asio/local.hpp"
#include "cocaine/asio/reactor.hpp"
#include "cocaine/asio/socket.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/manifest.hpp"
#include "cocaine/detail/profile.hpp"
#include "cocaine/detail/session.hpp"
#include "cocaine/detail/slave.hpp"
#include "cocaine/detail/unique_id.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/rpc/channel.hpp"

#include "cocaine/traits/json.hpp"
#include "cocaine/traits/literal.hpp"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/sum.hpp>

#include <boost/filesystem/operations.hpp>

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::io;

using namespace std::placeholders;

namespace {

struct ignore {
    void
    operator()(const std::error_code& /* ec */) const {
        // Do nothing.
    }
};

}

engine_t::engine_t(context_t& context,
                   const std::shared_ptr<reactor_t>& reactor,
                   const manifest_t& manifest,
                   const profile_t& profile,
                   const std::shared_ptr<io::socket<local>>& control):
    m_context(context),
    m_log(new logging::log_t(context, cocaine::format("app/%1%", manifest.name))),
    m_manifest(manifest),
    m_profile(profile),
    m_state(states::running),
    m_reactor(reactor),
    m_notification(new ev::async(m_reactor->native())),
    m_termination_timer(new ev::timer(m_reactor->native())),
    m_next_id(1)
{
    m_notification->set<engine_t, &engine_t::on_notification>(this);
    m_notification->start();

    const auto endpoint = local::endpoint(m_manifest.endpoint);

    if(boost::filesystem::exists(m_manifest.endpoint)) {
        // If we already have the endpoint path on filesystem, then just remove it.
        // The reason for it to exist is probably because the runtime has crashed
        // without being able to cleanup. We use composite endpoint paths with pid
        // of the process mixed in, so the only possible way for such collision to
        // happen is when the new runtime instance has the same pid as the crashed
        // one. Which is extremely rare, obviously.
        boost::filesystem::remove(m_manifest.endpoint);
    }

    m_connector.reset(new connector<acceptor<local>>(
        *m_reactor,
        std::make_unique<acceptor<local>>(endpoint)
    ));

    m_connector->bind(
        std::bind(&engine_t::on_connection, this, _1)
    );

    m_channel.reset(new channel<io::socket<local>>(
        *m_reactor,
        control
    ));

    m_channel->rd->bind(
        std::bind(&engine_t::on_control, this, _1),
        ignore()
    );

    m_channel->wr->bind(ignore());

    m_isolate = m_context.get<api::isolate_t>(
        m_profile.isolate.type,
        m_context,
        m_manifest.name,
        m_profile.isolate.args
    );
}

engine_t::~engine_t() {
    BOOST_ASSERT(m_state == states::stopped);
    boost::filesystem::remove(m_manifest.endpoint);
}

void
engine_t::run() {
    m_reactor->run();
}

std::shared_ptr<api::stream_t>
engine_t::enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream) {
    if(m_state != states::running) {
        throw cocaine::error_t("the engine is not active");
    }

    auto session = std::make_shared<session_t>(
        m_next_id++,
        event,
        upstream
    );

    {
        std::lock_guard<session_queue_t> queue_guard(m_queue);

        if(m_profile.queue_limit > 0 &&
           m_queue.size() >= m_profile.queue_limit)
        {
            throw cocaine::error_t("the queue is full");
        }

        m_queue.push(session);
    }

    wake();

    return std::make_shared<session_t::downstream_t>(session);
}

std::shared_ptr<api::stream_t>
engine_t::enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream, const std::string& tag) {
    if(m_state != states::running) {
        throw cocaine::error_t("the engine is not active");
    }

    auto session = std::make_shared<session_t>(
        m_next_id++,
        event,
        upstream
    );

    pool_map_t::iterator it;

    {
        std::lock_guard<std::mutex> pool_guard(m_pool_mutex);

        it = m_pool.find(tag);

        if(it == m_pool.end()) {
            if(m_pool.size() >= m_profile.pool_limit) {
                throw cocaine::error_t("the pool is full");
            }

            std::tie(it, std::ignore) = m_pool.insert(std::make_pair(
                tag,
                std::make_shared<slave_t>(m_context, *m_reactor, m_manifest, m_profile, tag, *this)
            ));
        }
    }

    it->second->assign(session);

    return std::make_shared<session_t::downstream_t>(session);
}

void
engine_t::erase(const std::string& id, int code, const std::string& reason) {
    std::lock_guard<std::mutex> pool_guard(m_pool_mutex);

    m_pool.erase(id);

    if(code == rpc::terminate::abnormal) {
        COCAINE_LOG_ERROR(m_log, "the app seems to be broken - %s", reason);
        migrate(states::broken);
    }

    if(m_state != states::running && m_pool.empty()) {
        // If it was the last slave, shut the engine down.
        stop();
    } else {
        wake();
    }
}

void
engine_t::wake() {
    m_notification->send();
}

void
engine_t::on_connection(const std::shared_ptr<io::socket<local>>& socket_) {
    const int fd = socket_->fd();

    COCAINE_LOG_DEBUG(m_log, "initiating a slave handshake on fd %d", fd);

    auto channel_ = std::make_shared<channel<io::socket<local>>>(*m_reactor, socket_);

    channel_->rd->bind(
        std::bind(&engine_t::on_handshake,  this, fd, _1),
        std::bind(&engine_t::on_disconnect, this, fd, _1)
    );

    channel_->wr->bind(
        std::bind(&engine_t::on_disconnect, this, fd, _1)
    );

    m_backlog[fd] = channel_;
}

void
engine_t::on_handshake(int fd, const message_t& message) {
    std::string id;
    backlog_t::mapped_type channel_ = m_backlog[fd];

    // Pop the channel.
    m_backlog.erase(fd);

    try {
        message.as<rpc::handshake>(id);
    } catch(const std::system_error& e) {
        COCAINE_LOG_WARNING(m_log, "disconnecting an incompatible slave on fd %d", fd);
        return;
    }

    pool_map_t::iterator it;

    {
        std::lock_guard<std::mutex> pool_guard(m_pool_mutex);

        it = m_pool.find(id);

        if(it == m_pool.end()) {
            COCAINE_LOG_WARNING(m_log, "disconnecting an unknown slave %s on fd %d", id, fd);
            return;
        }
    }

    COCAINE_LOG_DEBUG(m_log, "slave %s connected on fd %d", id, fd);

    it->second->bind(channel_);
}

void
engine_t::on_disconnect(int fd, const std::error_code& ec) {
    COCAINE_LOG_INFO(
        m_log,
        "slave has disconnected during the handshake on fd %d - [%d] %s",
        fd,
        ec.value(),
        ec.message()
    );

    m_backlog.erase(fd);
}

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

}

void
engine_t::on_control(const message_t& message) {
    std::lock_guard<std::mutex> pool_guard(m_pool_mutex);

    switch(message.id()) {
    case event_traits<control::report>::id: {
        collector_t collector;
        Json::Value info(Json::objectValue);

        size_t active = std::count_if(
            m_pool.cbegin(),
            m_pool.cend(),
            std::bind<bool>(std::ref(collector), _1)
        );

        info["load-median"] = static_cast<Json::LargestUInt>(collector.median());
        info["queue"]["capacity"] = static_cast<Json::LargestUInt>(m_profile.queue_limit);
        info["queue"]["depth"] = static_cast<Json::LargestUInt>(m_queue.size());
        info["sessions"]["pending"] = static_cast<Json::LargestUInt>(collector.sum());
        info["slaves"]["active"] = static_cast<Json::LargestUInt>(active);
        info["slaves"]["capacity"] = static_cast<Json::LargestUInt>(m_profile.pool_limit);
        info["slaves"]["idle"] = static_cast<Json::LargestUInt>(m_pool.size() - active);
        info["state"] = describe[static_cast<int>(m_state)];

        m_channel->wr->write<control::info>(0UL, info);
    } break;

    case event_traits<control::terminate>::id: {
        // Prepare for the shutdown.
        migrate(states::stopping);

        // NOTE: This message is needed to wake up the app's event loop, which is blocked
        // in order to allow the stream to flush the message queue.
        m_channel->wr->write<control::terminate>(0UL);
    } break;

    default:
        COCAINE_LOG_ERROR(m_log, "dropping unknown type %d control message", message.id());
    }
}

void
engine_t::on_notification(ev::async&, int) {
    pump();
    balance();
}

void
engine_t::on_termination(ev::timer&, int) {
    std::lock_guard<session_queue_t> queue_guard(m_queue);

    COCAINE_LOG_WARNING(m_log, "forcing the engine termination");

    stop();
}

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
            std::lock_guard<session_queue_t> queue_guard(m_queue);

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

    COCAINE_LOG_INFO(
        m_log,
        "enlarging the pool from %d to %d slaves",
        m_pool.size(),
        target
    );

    while(m_pool.size() != target) {
        const auto id = unique_id_t().string();

        try {
            m_pool.insert(std::make_pair(
                id,
                std::make_shared<slave_t>(m_context, *m_reactor, m_manifest, m_profile, id, *this)
            ));
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(m_log, "unable to spawn more slaves - [%d] %s", e.code().value(), e.code().message());
            break;
        } catch(const std::exception& e) {
            COCAINE_LOG_ERROR(m_log, "unable to spawn more slaves - %s", e.what());
            break;
        } catch(...) {
            COCAINE_LOG_ERROR(m_log, "unable to spawn more slaves - unknown exception");
            break;
        }
    }
}

void
engine_t::migrate(states target) {
    std::lock_guard<session_queue_t> queue_guard(m_queue);

    m_state = target;

    if(!m_queue.empty()) {
        COCAINE_LOG_DEBUG(
            m_log,
            "dropping %llu incomplete %s due to the engine state migration",
            m_queue.size(),
            m_queue.size() == 1 ? "session" : "sessions"
        );

        // Abort all the outstanding sessions.
        while(!m_queue.empty()) {
            m_queue.front()->upstream->error(
                resource_error,
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
    }

    COCAINE_LOG_INFO(
        m_log,
        "waiting for %d active %s to terminate, timeout: %.02f seconds",
        pending,
        pending == 1 ? "slave" : "slaves",
        m_profile.termination_timeout
    );

    m_termination_timer->set<engine_t, &engine_t::on_termination>(this);
    m_termination_timer->start(m_profile.termination_timeout);
}

void
engine_t::stop() {
    m_termination_timer->stop();

    // NOTE: This will force the slave pool termination.
    m_pool.clear();

    if(m_state == states::stopping) {
        m_state = states::stopped;

        // Don't stop the event loop if the engine is becoming broken.
        m_reactor->stop();
    }
}
