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

#include "cocaine/detail/actor.hpp"

#include "cocaine/api/service.hpp"
#include "cocaine/api/stream.hpp"

#include "cocaine/asio/acceptor.hpp"
#include "cocaine/asio/connector.hpp"
#include "cocaine/asio/reactor.hpp"
#include "cocaine/asio/socket.hpp"
#include "cocaine/asio/tcp.hpp"

#include "cocaine/context.hpp"
#include "cocaine/dispatch.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/rpc/channel.hpp"

#include "cocaine/traits/literal.hpp"

#if defined(__linux__)
    #include <sys/prctl.h>
#endif

using namespace cocaine;
using namespace cocaine::io;

using namespace std::placeholders;

struct actor_t::session_t {
    friend class actor_t;

    session_t(std::unique_ptr<io::channel<io::socket<io::tcp>>>&& ptr_, const std::shared_ptr<dispatch_t>& prototype_):
        ptr(std::move(ptr_)),
        prototype(prototype_)
    { }

    void
    detach(uint64_t tag) {
        downstreams.erase(tag);
    }

private:
    void
    invoke(const message_t& message, const std::shared_ptr<session_t>& self) {
        std::shared_ptr<downstream_t> downstream;

        {
            std::lock_guard<std::mutex> guard(mutex);

            auto it = downstreams.find(message.band());

            if(it == downstreams.end()) {
                std::tie(it, std::ignore) = downstreams.insert({ message.band(), std::make_shared<downstream_t>(
                    prototype,
                    std::make_shared<upstream_t>(self, message.band())
                )});
            }

            // NOTE: The downstream pointer is copied here so that if the slot decides to close the
            // downstream, it won't destroy it inside the downstream_t::invoke(). Instead, it will
            // be destroyed when this function scope is exited, liberating us from thinking of some
            // voodoo magic to handle it.
            downstream = it->second;
        }

        downstream->invoke(message);
    }

    void
    destroy() {
        std::lock_guard<std::mutex> guard(mutex);

        // This closes all the downstreams.
        downstreams.clear();

        // NOTE: This invalidates the internal channel pointer, but the wrapping lockable state
        // might still be accessible via upstreams in other threads.
        ptr.reset();
    }

private:
    std::unique_ptr<io::channel<io::socket<io::tcp>>> ptr;
    std::mutex mutex;

    // Root dispatch

    const std::shared_ptr<dispatch_t> prototype;

    // Downstreams

    struct downstream_t {
        downstream_t(const std::shared_ptr<dispatch_t>& d, const std::shared_ptr<upstream_t>& u):
            dispatch(d),
            upstream(u)
        { }

        void
        invoke(const message_t& message);

        // Active protocol for this downstream.
        std::shared_ptr<dispatch_t> dispatch;

        // As of now, all clients are considered using the single streaming protocol, so upstreams
        // don't change when the downstream protocol is switched over.
        std::shared_ptr<upstream_t> upstream;
    };

    std::map<uint64_t, std::shared_ptr<downstream_t>> downstreams;
};

struct actor_t::upstream_t:
    public api::stream_t
{
    upstream_t(const std::shared_ptr<session_t>& session, uint64_t tag):
        m_state(state::open),
        m_session(session),
        m_tag(tag)
    { }

    virtual
    void
    write(const char* chunk, size_t size) {
        std::lock_guard<std::mutex> guard(m_session->mutex);

        if(m_state == state::open && m_session->ptr) {
            m_session->ptr->wr->write<rpc::chunk>(m_tag, literal { chunk, size });
        }
    }

    virtual
    void
    error(int code, const std::string& reason) {
        std::lock_guard<std::mutex> guard(m_session->mutex);

        if(m_state == state::open && m_session->ptr) {
            m_session->ptr->wr->write<rpc::error>(m_tag, code, reason);
        }
    }

    virtual
    void
    close() {
        std::lock_guard<std::mutex> guard(m_session->mutex);

        if(m_state == state::open) {
            if(m_session->ptr) {
                m_session->ptr->wr->write<rpc::choke>(m_tag);
            }

            // Destroys the session with the given tag in the stream, so that new requests might
            // reuse the tag in the future.
            m_session->detach(m_tag);

            m_state = state::closed;
        }
    }

private:
    struct state {
        enum value: int { open, closed };
    };

    // Upstream state.
    state::value m_state;

    const std::shared_ptr<session_t> m_session;
    const uint64_t m_tag;
};

void
actor_t::session_t::downstream_t::invoke(const message_t& message) {
    try {
        if(!dispatch)
            // TODO: COCAINE-82 changes to 'client' error category.
            throw cocaine::error_t("downstream has been closed");
        dispatch = dispatch->invoke(message, upstream);
    } catch(const std::exception& e) {
        // TODO: COCAINE-82 changes to a category-based exception serialization.
        upstream->error(invocation_error, e.what());
        upstream->close();
    }
}

actor_t::actor_t(context_t& context, std::shared_ptr<reactor_t> reactor, std::unique_ptr<dispatch_t>&& prototype):
    m_context(context),
    m_log(new logging::log_t(context, prototype->name())),
    m_reactor(reactor),
    m_prototype(std::move(prototype))
{ }

actor_t::actor_t(context_t& context, std::shared_ptr<reactor_t> reactor, std::unique_ptr<api::service_t>&& service):
    m_context(context),
    m_log(new logging::log_t(context, service->prototype().name())),
    m_reactor(reactor)
{
    std::shared_ptr<api::service_t> enclosure(std::move(service));

    m_prototype = std::shared_ptr<dispatch_t>(
        enclosure,
       &enclosure->prototype()
    );
}

actor_t::~actor_t() {
    m_prototype.reset();

    for(auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        // Synchronously close the channels.
        it->second->destroy();
    }
}

namespace {

struct named_runnable {
    void
    operator()() const {
#if defined(__linux__)
        if(name.size() < 16) {
            ::prctl(PR_SET_NAME, name.c_str());
        } else {
            ::prctl(PR_SET_NAME, name.substr(0, 16).data());
        }
#endif

        reactor->run();
    }

    const std::string name;
    const std::shared_ptr<reactor_t>& reactor;
};

}

void
actor_t::run(std::vector<tcp::endpoint> endpoints) {
    BOOST_ASSERT(!m_thread);

    for(auto it = endpoints.cbegin(); it != endpoints.cend(); ++it) {
        m_connectors.emplace_back(
            *m_reactor,
            std::make_unique<acceptor<tcp>>(*it)
        );

        m_connectors.back().bind(std::bind(&actor_t::on_connection, this, _1));
    }

    m_thread.reset(new std::thread(named_runnable {
        m_prototype->name(),
        m_reactor
    }));
}

void
actor_t::terminate() {
    BOOST_ASSERT(m_thread);

    m_reactor->post(std::bind(&reactor_t::stop, m_reactor));

    m_thread->join();
    m_thread.reset();

    m_connectors.clear();
}

std::vector<tcp::endpoint>
actor_t::location() const {
    BOOST_ASSERT(!m_connectors.empty());

    std::vector<tcp::endpoint> endpoints;

    for(auto it = m_connectors.begin(); it != m_connectors.end(); ++it) {
        endpoints.push_back(it->endpoint());
    }

    return endpoints;
}

dispatch_t&
actor_t::dispatch() {
    return *m_prototype;
}

auto
actor_t::metadata() const -> metadata_t {
    const auto port = location().front().port();
    const auto endpoint = locator::endpoint_tuple_type(m_context.config.network.hostname, port);

    return metadata_t(
        endpoint,
        m_prototype->versions(),
        m_prototype->protocol()
    );
}

auto
actor_t::counters() const -> counters_t {
    counters_t result;

    result.sessions = m_sessions.size();

    for(auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        std::lock_guard<std::mutex> guard(it->second->mutex);

        auto info = std::make_tuple(
            it->second->downstreams.size(),
            it->second->ptr->footprint()
        );

        result.footprints.insert({ it->second->ptr->remote_endpoint(), info });
    }

    return result;
}

void
actor_t::on_connection(const std::shared_ptr<io::socket<tcp>>& socket_) {
    const int fd = socket_->fd();

    BOOST_ASSERT(m_channels.find(fd) == m_channels.end());

    COCAINE_LOG_DEBUG(m_log, "accepted a new client from '%s' on fd %d", socket_->remote_endpoint(), fd);

    auto ptr = std::make_unique<channel<io::socket<tcp>>>(*m_reactor, socket_);

    ptr->rd->bind(
        std::bind(&actor_t::on_message, this, fd, _1),
        std::bind(&actor_t::on_failure, this, fd, _1)
    );

    ptr->wr->bind(
        std::bind(&actor_t::on_failure, this, fd, _1)
    );

    m_sessions[fd] = std::make_unique<session_t>(std::move(ptr), m_prototype);
}

void
actor_t::on_message(int fd, const message_t& message) {
    auto it = m_sessions.find(fd);

    BOOST_ASSERT(it != m_sessions.end());

    it->second->invoke(message, it->second);
}

void
actor_t::on_failure(int fd, const std::error_code& ec) {
    auto it = m_sessions.find(fd);

    if(it == m_sessions.end()) {
        // TODO: COCAINE-75 fixes this via cancellation.
        // Check whether the channel actually exists, in case multiple errors were queued up in the
        // reactor and it was already dropped.
        return;
    } else if(ec) {
        COCAINE_LOG_ERROR(m_log, "client on fd %d has disappeared - [%d] %s", fd, ec.value(), ec.message());
    } else {
        COCAINE_LOG_DEBUG(m_log, "client on fd %d has disconnected", fd);
    }

    // This destroys the channel but not the wrapping lockable state.
    it->second->destroy();

    // This doesn't guarantee that the wrapping lockable state will be deleted, as it can be shared
    // with other threads via upstreams, but it's fine since the channel is destroyed.
    m_sessions.erase(it);
}
