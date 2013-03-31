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
using namespace cocaine::logging;

using namespace std::placeholders;

// Session queue

void
session_queue_t::push(const_reference session) {
    if(session->event.policy.urgent) {
        emplace_front(session);
    } else {
        emplace_back(session);
    }
}

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
                ptr->send<rpc::error>(static_cast<int>(code), message);
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

        enum class states: int {
            open,
            closed
        };

        states m_state;
    };
}

// Engine

namespace {
    struct ignore_t {
        void
        operator()(const std::error_code& /* ec */) {
            // Do nothing.
        }
    };
}

engine_t::engine_t(context_t& context,
                   std::unique_ptr<reactor_t>&& reactor,
                   const manifest_t& manifest,
                   const profile_t& profile,
                   const std::shared_ptr<io::socket<local>>& control):
    m_context(context),
    m_log(new log_t(context, cocaine::format("app/%1%", manifest.name))),
    m_manifest(manifest),
    m_profile(profile),
    m_state(states::stopped),
    m_reactor(std::move(reactor)),
    m_gc_timer(m_reactor->native()),
    m_termination_timer(m_reactor->native()),
    m_notification(m_reactor->native()),
    m_next_id(1)
{
    m_gc_timer.set<engine_t, &engine_t::on_cleanup>(this);
    m_gc_timer.start(5.0f, 5.0f);

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
        ignore_t()
    );

    m_channel->wr->bind(ignore_t());

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
engine_t::enqueue(const api::event_t& event,
                  const std::shared_ptr<api::stream_t>& upstream)
{
    auto session = std::make_shared<session_t>(
        m_next_id++,
        event,
        upstream
    );

    {
        std::unique_lock<session_queue_t> lock(m_queue);

        if(m_state != states::running) {
            throw cocaine::error_t("the engine is not active");
        }

        if(m_profile.queue_limit > 0 &&
           m_queue.size() >= m_profile.queue_limit)
        {
            throw cocaine::error_t("the queue is full");
        }

        m_queue.push(session);
    }

    // NOTE: This will probably go to the session cache, but we save
    // on this serialization later.
    session->send<rpc::invoke>(event.name);

    // Pump the queue!
    wake();

    return std::make_shared<downstream_t>(session);
}

void
engine_t::erase(const unique_id_t& uuid,
                int code,
                const std::string& /* reason */)
{
    pool_map_t::iterator it = m_pool.find(uuid);

    BOOST_ASSERT(it != m_pool.end());

    m_pool.erase(it);

    if(code == rpc::terminate::abnormal) {
        COCAINE_LOG_ERROR(m_log, "the app seems to be broken - stopping");
        migrate(states::broken);
    }

    if(m_state != states::running && m_pool.empty()) {
        // If it was the last slave, shut the engine down.
        stop();
    }
}

void
engine_t::on_connection(const std::shared_ptr<io::socket<local>>& socket_) {
    auto channel_ = std::make_shared<channel<io::socket<local>>>(*m_reactor, socket_);

    channel_->rd->bind(
        std::bind(&engine_t::on_handshake, this, channel_, _1),
        std::bind(&engine_t::on_disconnect, this, channel_, _1)
    );

    channel_->wr->bind(
        std::bind(&engine_t::on_disconnect, this, channel_, _1)
    );

    COCAINE_LOG_DEBUG(m_log, "initiating a slave handshake on fd: %d", socket_->fd());

    m_backlog.insert(channel_);
}

void
engine_t::on_handshake(const std::shared_ptr<channel<io::socket<local>>>& channel_,
                       const message_t& message)
{
    unique_id_t uuid(uninitialized);

    try {
        std::string blob;
        message.as<rpc::handshake>(blob);
        uuid = unique_id_t(blob);
    } catch(const cocaine::error_t& e) {
        COCAINE_LOG_WARNING(m_log, "dropping an invalid handshake message");
        channel_->rd->unbind();
        channel_->wr->unbind();
        return;
    }

    pool_map_t::iterator it = m_pool.find(uuid);

    if(it == m_pool.end()) {
        COCAINE_LOG_WARNING(m_log, "dropping a handshake from an unknown slave %s", uuid);
        channel_->rd->unbind();
        channel_->wr->unbind();
        return;
    }

    m_backlog.erase(channel_);

    COCAINE_LOG_DEBUG(m_log, "slave %s connected", uuid);

    it->second->bind(channel_);

    wake();
}

void
engine_t::on_disconnect(const std::shared_ptr<channel<io::socket<local>>>& channel_,
                        const std::error_code& ec)
{
    COCAINE_LOG_INFO(
        m_log,
        "slave disconnected during the handshake - [%d] %s",
        ec.value(),
        ec.message()
    );

    channel_->rd->unbind();
    channel_->wr->unbind();

    m_backlog.erase(channel_);
}

namespace {
    static
    const char*
    describe[] = {
        "running",
        "broken",
        "stopping",
        "stopped"
    };
}

namespace {
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

            return slave.second->state() == slave_t::states::active &&
                   load;
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

        case event_traits<control::terminate>::id:
            migrate(states::stopping);

            // NOTE: This message is needed to wake up the app's event loop, which is blocked
            // in order to allow the stream to flush the message queue.
            m_channel->wr->write<control::terminate>(0UL);

            break;

        default:
            COCAINE_LOG_ERROR(m_log, "dropping unknown type %d control message", message.id());
    }
}

void
engine_t::on_cleanup(ev::timer&, int) {
    typedef std::vector<
        pool_map_t::key_type
    > corpse_list_t;

    corpse_list_t corpses;

    for(pool_map_t::iterator it = m_pool.begin(); it != m_pool.end(); ++it) {
        if(it->second->state() == slave_t::states::dead) {
            corpses.emplace_back(it->first);
        }
    }

    if(!corpses.empty()) {
        for(corpse_list_t::iterator it = corpses.begin();
            it != corpses.end();
            ++it)
        {
            m_pool.erase(*it);
        }

        COCAINE_LOG_DEBUG(
            m_log,
            "recycled %llu dead %s",
            corpses.size(),
            corpses.size() == 1 ? "slave" : "slaves"
        );
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
    struct load_t {
        template<class T>
        bool
        operator()(const T& lhs, const T& rhs) const {
            return lhs.second->load() < rhs.second->load();
        }
    };

    struct available_t {
        available_t(size_t max_):
            max(max_)
        { }

        template<class T>
        bool
        operator()(const T& slave) const {
            return slave.second->state() == slave_t::states::active &&
                   slave.second->load() < max;
        }

        const size_t max;
    };

    template<class It, class Compare, class Predicate>
    inline
    It
    min_element_if(It first,
                   It last,
                   Compare compare,
                   Predicate predicate)
    {
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
        pool_map_t::iterator it = min_element_if(
            m_pool.begin(),
            m_pool.end(),
            load_t(),
            available_t(m_profile.concurrency)
        );

        if(it == m_pool.end()) {
            return;
        }

        session_queue_t::value_type session;

        do {
            std::unique_lock<session_queue_t> lock(m_queue);

            if(m_queue.empty()) {
                return;
            }

            session = m_queue.front();
            m_queue.pop_front();

            // Process the queue head outside the lock, because it might take
            // some considerable amount of time if, for example, the session has
            // expired and there's some heavy-lifting in the error handler.
            lock.unlock();

            if(session->event.policy.deadline &&
               session->event.policy.deadline <= m_reactor->native().now())
            {
                COCAINE_LOG_DEBUG(
                    m_log,
                    "session %s has expired, dropping",
                    session->id
                );

                session->upstream->error(
                    deadline_error,
                    "the session has expired in the queue"
                );

                session.reset();
            }
        } while(!session);

        // Attach the session to the worker.
        it->second->assign(std::move(session));
    }
}

void
engine_t::balance() {
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
        try {
            std::shared_ptr<slave_t> slave(
                std::make_shared<slave_t>(
                    m_context,
                    *m_reactor,
                    m_manifest,
                    m_profile,
                    *this
                )
            );

            m_pool.emplace(slave->id(), slave);
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

    for(pool_map_t::iterator it = m_pool.begin();
        it != m_pool.end();
        ++it)
    {
        if(it->second->state() == slave_t::states::active) {
            it->second->stop();
            ++pending;
        }
    }

    if(pending) {
        COCAINE_LOG_INFO(
            m_log,
            "waiting for %d active %s to terminate, timeout: %.02f seconds",
            pending,
            pending == 1 ? "slave" : "slaves",
            m_profile.termination_timeout
        );

        m_termination_timer.set<engine_t, &engine_t::on_termination>(this);
        m_termination_timer.start(m_profile.termination_timeout);
    } else {
        stop();
    }
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
