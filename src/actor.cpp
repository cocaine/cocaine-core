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

struct actor_t::lockable_t {
    friend class actor_t;

    lockable_t(std::unique_ptr<io::channel<io::socket<io::tcp>>>&& ptr_):
        ptr(std::move(ptr_))
    { }

private:
    void
    destroy() {
        std::lock_guard<std::mutex> guard(mutex);

        // NOTE: This invalidates the internal channel pointer, but the wrapping lockable state
        // might still be accessible via upstreams in other threads.
        ptr.reset();
    }

    std::unique_ptr<io::channel<io::socket<io::tcp>>> ptr;
    std::mutex mutex;
};

struct actor_t::upstream_t:
    public api::stream_t
{
    upstream_t(const std::shared_ptr<lockable_t>& channel, uint64_t tag):
        m_state(state::open),
        m_channel(channel),
        m_tag(tag)
    { }

    virtual
    void
    write(const char* chunk, size_t size) {
        std::lock_guard<std::mutex> guard(m_channel->mutex);

        if(m_state == state::open && m_channel->ptr) {
            m_channel->ptr->wr->write<rpc::chunk>(m_tag, literal { chunk, size });
        }
    }

    virtual
    void
    error(int code, const std::string& reason) {
        std::lock_guard<std::mutex> guard(m_channel->mutex);

        if(m_state == state::open && m_channel->ptr) {
            m_channel->ptr->wr->write<rpc::error>(m_tag, code, reason);
        }
    }

    virtual
    void
    close() {
        std::lock_guard<std::mutex> guard(m_channel->mutex);

        if(m_state == state::open) {
            if(m_channel->ptr) {
                m_channel->ptr->wr->write<rpc::choke>(m_tag);
            }

            m_state = state::closed;
        }
    }

private:
    struct state {
        enum value: int { open, closed };
    };

    // Upstream state.
    state::value m_state;

    const std::shared_ptr<lockable_t> m_channel;
    const uint64_t m_tag;
};

actor_t::actor_t(context_t& context, std::shared_ptr<reactor_t> reactor, std::unique_ptr<dispatch_t>&& dispatch):
    m_context(context),
    m_log(new logging::log_t(context, dispatch->name())),
    m_reactor(reactor),
    m_dispatch(std::move(dispatch))
{ }

actor_t::~actor_t() {
    m_dispatch.reset();

    for(auto it = m_channels.cbegin(); it != m_channels.cend(); ++it) {
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
        m_dispatch->name(),
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
    return *m_dispatch;
}

auto
actor_t::metadata() const -> metadata_t {
    const auto port = location().front().port();
    const auto endpoint = locator::endpoint_tuple_type(m_context.config.network.hostname, port);

    return metadata_t(
        endpoint,
        m_dispatch->version(),
        m_dispatch->map()
    );
}

auto
actor_t::counters() const -> counters_t {
    counters_t result;

    result.channels = m_channels.size();

    for(auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        std::lock_guard<std::mutex> guard(it->second->mutex);

        result.footprints.insert({
            it->second->ptr->remote_endpoint(),
            it->second->ptr->footprint()
        });
    }

    return result;
}

void
actor_t::on_connection(const std::shared_ptr<io::socket<tcp>>& socket_) {
    const int fd = socket_->fd();

    BOOST_ASSERT(m_channels.find(fd) == m_channels.end());

    try {
        COCAINE_LOG_DEBUG(m_log, "accepted a new client from '%s' on fd %d", socket_->remote_endpoint(), fd);
    } catch(const std::system_error&) {
        COCAINE_LOG_DEBUG(m_log, "client on fd %d has disappeared", fd);
        return;
    }

    auto ptr = std::make_unique<channel<io::socket<tcp>>>(*m_reactor, socket_);

    ptr->rd->bind(
        std::bind(&actor_t::on_message, this, fd, _1),
        std::bind(&actor_t::on_failure, this, fd, _1)
    );

    ptr->wr->bind(
        std::bind(&actor_t::on_failure, this, fd, _1)
    );

    m_channels[fd] = std::make_shared<lockable_t>(std::move(ptr));
}

void
actor_t::on_message(int fd, const message_t& message) {
    auto it = m_channels.find(fd);

    BOOST_ASSERT(it != m_channels.end());

    m_dispatch->invoke(message, std::make_shared<upstream_t>(
        it->second,
        message.band()
    ));
}

void
actor_t::on_failure(int fd, const std::error_code& ec) {
    auto it = m_channels.find(fd);

    if(it == m_channels.end()) {
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
    m_channels.erase(it);
}
