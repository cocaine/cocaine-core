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
#include "cocaine/asio/socket.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/manifest.hpp"
#include "cocaine/detail/profile.hpp"
#include "cocaine/detail/session.hpp"
#include "cocaine/detail/slave.hpp"
#include "cocaine/detail/unique_id.hpp"

#include "cocaine/detail/traits/json.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/rpc/channel.hpp"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/sum.hpp>

#include <boost/filesystem/operations.hpp>

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::io;

using namespace std::placeholders;

// Downstream

namespace {
    struct downstream_t:
        public api::stream_t
    {
        downstream_t(const std::shared_ptr<session_t>& session):
            m_session(session),
            m_state(states::open)
        { }

        virtual
       ~downstream_t() {
            if(m_state != states::closed) {
                close();
            }
        }

        virtual
        void
        write(const char* chunk, size_t size) {
            if(m_state == states::closed) {
                throw cocaine::error_t("the stream has been closed");
            }

            const std::shared_ptr<session_t> ptr = m_session.lock();

            if(ptr) {
                ptr->send<rpc::chunk>(std::string(chunk, size));
            }
        }

        virtual
        void
        error(error_code code, const std::string& message) {
            if(m_state == states::closed) {
                throw cocaine::error_t("the stream has been closed");
            }

            m_state = states::closed;

            const std::shared_ptr<session_t> ptr = m_session.lock();

            if(ptr) {
                ptr->send<rpc::error>(code, message);
                ptr->send<rpc::choke>();
            }
        }

        virtual
        void
        close() {
            if(m_state == states::closed) {
                throw cocaine::error_t("the stream has been closed");
            }

            m_state = states::closed;

            const std::shared_ptr<session_t> ptr = m_session.lock();

            if(ptr) {
                ptr->send<rpc::choke>();
            }
        }

    private:
        const std::weak_ptr<session_t> m_session;

        enum class states {
            open,
            closed
        };

        states m_state;
    };
}

// Engine

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
    m_state(states::stopped),
    m_reactor(reactor),
    m_notification(m_reactor->native()),
    m_termination_timer(m_reactor->native()),
    m_next_id(1)
{
    m_notification.set<engine_t, &engine_t::on_notification>(this);
    m_notification.start();

    auto endpoint = local::endpoint(m_manifest.endpoint);

    m_connector.reset(new connector<acceptor<local>>(
        *m_reactor,
        std::unique_ptr<acceptor<local>>(new acceptor<local>(endpoint))
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
    m_state = states::running;
    m_reactor->run();
}

void
engine_t::wake() {
    m_notification.send();
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
        std::unique_lock<session_queue_t> lock(m_queue);

        if(m_profile.queue_limit > 0 &&
           m_queue.size() >= m_profile.queue_limit)
        {
            throw cocaine::error_t("the queue is full");
        }

        m_queue.push(session);
    }

    // Pump the queue!
    wake();

    // NOTE: This will probably go to the session cache, but we save on this serialization later.
    session->send<rpc::invoke>(event.name);

    return std::make_shared<downstream_t>(session);
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

    {
        std::unique_lock<std::mutex> lock(m_pool_mutex);

        auto it = m_pool.find(tag);

        if(it == m_pool.end()) {
            if(m_pool.size() >= m_profile.pool_limit) {
                throw cocaine::error_t("the pool is full");
            }

            std::tie(it, std::ignore) = m_pool.emplace(
                tag,
                std::make_shared<slave_t>(m_context, *m_reactor, m_manifest, m_profile, tag, *this)
            );
        }

        it->second->assign(session);
    }

    // NOTE: This will probably go to the session cache, but we save on this serialization later.
    session->send<rpc::invoke>(event.name);

    return std::make_shared<downstream_t>(session);
}

void
engine_t::erase(const std::string& id, int code, const std::string& reason) {
    std::unique_lock<std::mutex> lock(m_pool_mutex);

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
engine_t::on_connection(const std::shared_ptr<io::socket<local>>& socket_) {
    auto fd = socket_->fd();
    auto channel_ = std::make_shared<channel<io::socket<local>>>(*m_reactor, socket_);

    channel_->rd->bind(
        std::bind(&engine_t::on_handshake,  this, fd, _1),
        std::bind(&engine_t::on_disconnect, this, fd, _1)
    );

    channel_->wr->bind(
        std::bind(&engine_t::on_disconnect, this, fd, _1)
    );

    COCAINE_LOG_DEBUG(m_log, "initiating a slave handshake on fd %d", socket_->fd());

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
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_WARNING(m_log, "disconnecting a malfunctioning slave on fd %d", fd);
        return;
    }

    std::unique_lock<std::mutex> lock(m_pool_mutex);

    pool_map_t::iterator it = m_pool.find(id);

    if(it == m_pool.end()) {
        COCAINE_LOG_WARNING(m_log, "disconnecting an unknown slave %s on fd %d", id, fd);
        return;
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
    static
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
            size_t load = slave.second->load();

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
    switch(message.id()) {
        case event_traits<control::report>::id: {
            Json::Value info(Json::objectValue);

            collector_t collector;

            std::unique_lock<std::mutex> lock(m_pool_mutex);

            size_t active = std::count_if(
                m_pool.begin(),
                m_pool.end(),
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

            break;
        }

        case event_traits<control::terminate>::id: {
            std::unique_lock<std::mutex> lock(m_pool_mutex);

            // Prepare for the shutdown.
            migrate(states::stopping);

            // NOTE: This message is needed to wake up the app's event loop, which is blocked
            // in order to allow the stream to flush the message queue.
            m_channel->wr->write<control::terminate>(0UL);

            break;
        }

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
    std::unique_lock<session_queue_t> lock(m_queue);

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
            return first;
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
    while(!m_queue.empty()) {
        std::unique_lock<std::mutex> pool_lock(m_pool_mutex);

        auto it = min_element_if(m_pool.begin(), m_pool.end(), load(), available {
            m_profile.concurrency
        });

        if(it == m_pool.end()) {
            return;
        }

        std::unique_lock<session_queue_t> queue_lock(m_queue);

        if(m_queue.empty()) {
            return;
        }

        session_queue_t::value_type session = m_queue.front();
        m_queue.pop_front();

        // Process the queue head outside the lock, because it might take some considerable amount
        // of time if the session has expired and there's some heavy-lifting in the error handler.
        queue_lock.unlock();

        // Attach the session to the slave.
        it->second->assign(session);
    }
}

void
engine_t::balance() {
    std::unique_lock<std::mutex> lock(m_pool_mutex);

    if(m_pool.size() >= m_profile.pool_limit ||
       m_pool.size() * m_profile.grow_threshold >= m_queue.size())
    {
        return;
    }

    unsigned int target = std::min(
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
        auto id = unique_id_t().string();

        try {
            m_pool.emplace(
                id,
                std::make_shared<slave_t>(
                    m_context,
                    *m_reactor,
                    m_manifest,
                    m_profile,
                    id,
                    *this
                )
            );
        } catch(const cocaine::error_t& e) {
            COCAINE_LOG_ERROR(m_log, "unable to spawn more slaves - %s", e.what());
            break;
        }
    }
}

void
engine_t::migrate(states target) {
    std::unique_lock<session_queue_t> lock(m_queue);

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
        return stop();
    }

    COCAINE_LOG_INFO(
        m_log,
        "waiting for %d active %s to terminate, timeout: %.02f seconds",
        pending,
        pending == 1 ? "slave" : "slaves",
        m_profile.termination_timeout
    );

    m_termination_timer.set<engine_t, &engine_t::on_termination>(this);
    m_termination_timer.start(m_profile.termination_timeout);
}

void
engine_t::stop() {
    if(m_termination_timer.is_active()) {
        m_termination_timer.stop();
    }

    // NOTE: This will force the slave pool termination.
    m_pool.clear();

    if(m_state == states::stopping) {
        m_state = states::stopped;
        m_reactor->stop();
    }
}
