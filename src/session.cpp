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

#include "cocaine/rpc/session.hpp"

#include "cocaine/hpack/static_table.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/rpc/asio/transport.hpp"

#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/upstream.hpp"

#include <asio/ip/tcp.hpp>
#include <asio/local/stream_protocol.hpp>

#include <blackhole/logger.hpp>

using namespace cocaine;
using namespace cocaine::io;

using namespace asio;

// Session internals

class session_t::pull_action_t:
    public std::enable_shared_from_this<pull_action_t>
{
    decoder_t::message_type message;

    // Keeps the session alive until all the operations are complete.
    const std::shared_ptr<session_t> session;

public:
    pull_action_t(const std::shared_ptr<session_t>& session_):
        session(session_)
    { }

    void
    operator()(const std::shared_ptr<transport_type> ptr);

private:
    void
    finalize(const std::error_code& ec);
};

void
session_t::pull_action_t::operator()(const std::shared_ptr<transport_type> ptr) {
    ptr->reader->read(message, std::bind(&pull_action_t::finalize,
        shared_from_this(),
        std::placeholders::_1
    ));
}

void
session_t::pull_action_t::finalize(const std::error_code& ec) {
    if(ec) {
        if(ec != asio::error::eof) {
            COCAINE_LOG_ERROR(session->log, "client disconnected: [{:d}] {}", ec.value(), ec.message());
        } else {
            COCAINE_LOG_DEBUG(session->log, "client disconnected");
        }

        return session->detach(ec);
    }

#if defined(__clang__)
    if(const auto ptr = std::atomic_load(&session->transport)) {
#else
    if(const auto ptr = *session->transport.synchronize()) {
#endif
        try {
            // NOTE: In case the underlying slot has miserably failed to handle its exceptions, the
            // client will be disconnected to prevent any further damage to the service and himself.
            session->handle(message);
            message.clear();
        } catch(const std::system_error& e) {
            COCAINE_LOG_ERROR(session->log, "uncaught invocation exception: {}", error::to_string(e));
            return session->detach(e.code());
        } catch(const std::exception& e) {
            COCAINE_LOG_ERROR(session->log, "uncaught invocation exception: {}", e.what());
            return session->detach(error::uncaught_error);
        }

        // Cycle the transport back into the message pump.
        operator()(std::move(ptr));
    } else {
        COCAINE_LOG_DEBUG(session->log, "ignoring invocation due to detached session");
    }
}

class session_t::push_action_t:
    public enable_shared_from_this<push_action_t>
{
    const encoder_t::message_type message;

    // Keeps the session alive until all the operations are complete.
    const std::shared_ptr<session_t> session;

public:
    push_action_t(encoder_t::message_type&& message, const std::shared_ptr<session_t>& session_):
        message(std::move(message)),
        session(session_)
    { }

    void
    operator()(const std::shared_ptr<transport_type> ptr);

private:
    void
    finalize(const std::error_code& ec);
};

void
session_t::push_action_t::operator()(const std::shared_ptr<transport_type> ptr) {
    if(!trace_t::current().empty()) {
        if(trace_t::current().pushed()) {
            COCAINE_LOG_DEBUG(session->log, "cs");
        } else {
            COCAINE_LOG_DEBUG(session->log, "ss");
        }
    }

    ptr->writer->write(message, trace_t::bind(&push_action_t::finalize,
        shared_from_this(),
        std::placeholders::_1
    ));
}

void
session_t::push_action_t::finalize(const std::error_code& ec) {
    COCAINE_LOG_DEBUG(session->log, "after send");
    if(ec.value() == 0) return;

    if(ec != asio::error::eof) {
        COCAINE_LOG_ERROR(session->log, "client disconnected: [{:d}] {}", ec.value(), ec.message());
    } else {
        COCAINE_LOG_DEBUG(session->log, "client disconnected");
    }

    return session->detach(ec);
}

class session_t::channel_t
{
public:
    channel_t(const dispatch_ptr_t& dispatch_, const upstream_ptr_t& upstream_):
        dispatch(dispatch_),
        upstream(upstream_)
    { }

    dispatch_ptr_t dispatch;
    upstream_ptr_t upstream;
};

// Session

session_t::session_t(std::unique_ptr<logging::logger_t> log_, std::unique_ptr<transport_type> transport_, const dispatch_ptr_t& prototype_):
    log(std::move(log_)),
    transport(std::shared_ptr<transport_type>(std::move(transport_))),
    prototype(prototype_),
    max_channel_id(0)
{ }

// Operations

void
session_t::handle(const decoder_t::message_type& message) {
    const channel_map_t::key_type channel_id = message.span();
    boost::optional<trace_t> incoming_trace;

    const auto channel = channels.apply([&](channel_map_t& mapping) -> std::shared_ptr<channel_t> {
        channel_map_t::const_iterator lb, ub;

        std::tie(lb, ub) = mapping.equal_range(channel_id);

        if(lb == ub) {
            if(channel_id <= max_channel_id) {
                // NOTE: Checking whether channel number is always higher than the previous channel
                // number is similar to an infinite TIME_WAIT timeout for TCP sockets. It might be
                // not the best approach, but since we have 2^64 possible channels it's good enough.
                throw std::system_error(error::revoked_channel, std::to_string(channel_id));
            }

            std::tie(lb, std::ignore) = mapping.insert({channel_id, std::make_shared<channel_t>(
                prototype,
                // Do not store trace if we handling server side.
                std::make_shared<basic_upstream_t>(shared_from_this(), channel_id, boost::none)
            )});

            max_channel_id = channel_id;
        }
        if(lb->second->upstream->client_trace) {
            incoming_trace = lb->second->upstream->client_trace;
        } else {
            auto trace_header = message.meta<hpack::headers::trace_id<>>();
            auto span_header = message.meta<hpack::headers::span_id<>>();
            auto parent_header = message.meta<hpack::headers::parent_id<>>();
            if(trace_header && span_header && parent_header) {
                incoming_trace = trace_t(
                    trace_header->get_value().convert<uint64_t>(),
                    span_header->get_value().convert<uint64_t>(),
                    parent_header->get_value().convert<uint64_t>(),
                    std::get<0>(lb->second->dispatch->root().at(message.type()))
                );
            }
        }

        // NOTE: The virtual channel pointer is copied here to avoid data races.
        return lb->second;
    });

    if(!channel->dispatch) {
        throw std::system_error(error::unbound_dispatch);
    }

    trace_t::restore_scope_t trace_scope(incoming_trace);

    COCAINE_LOG_DEBUG(log, "invocation type {}: '{}' in channel {}, dispatch: '{}'",
        message.type(),
        channel->dispatch->root().count(message.type()) ?
            std::get<0>(channel->dispatch->root().at(message.type()))
          : "<undefined>",
        channel_id,
        channel->dispatch->name());

    if(!trace_t::current().empty()) {
        if(trace_t::current().pushed()) {
            COCAINE_LOG_DEBUG(log, "cr");
            trace_t::current().pop();
        } else {
            COCAINE_LOG_DEBUG(log, "sr");
        }
    }

    if((channel->dispatch = channel->dispatch->process(message, channel->upstream)
        .get_value_or(channel->dispatch)) == nullptr)
    {
        // NOTE: If the client has sent us the last message according to our dispatch graph, revoke
        // the channel. No-op if the channel is no longer in the mapping, e.g., was discarded during
        // session::detach(), which was called during the dispatch::process().
        if(!channel.unique()) revoke(channel_id);
    }
}

void
session_t::revoke(uint64_t channel_id) {
    channels.apply([&](channel_map_t& mapping) {
        auto it = mapping.find(channel_id);

        if(it == mapping.end()) {
            COCAINE_LOG_WARNING(log, "ignoring revoke request for channel {:d}", channel_id);
            return;
        }

        if(it->second->dispatch) {
            COCAINE_LOG_ERROR(log, "revoking channel {:d}, dispatch: '{}'", channel_id,
                it->second->dispatch->name());
            it->second->dispatch->discard(std::error_code());
        } else {
            COCAINE_LOG_DEBUG(log, "revoking channel {:d}", channel_id);
        }

        mapping.erase(it);
    });
}

upstream_ptr_t
session_t::fork(const dispatch_ptr_t& dispatch) {
    return channels.apply([&](channel_map_t& mapping) -> upstream_ptr_t {
        const auto channel_id = ++max_channel_id;
        auto trace = trace_t::current();
        trace.push(dispatch->name());
        const auto downstream = std::make_shared<basic_upstream_t>(shared_from_this(), channel_id, trace);

        COCAINE_LOG_DEBUG(log, "forking new channel {:d}, dispatch: '{}'", channel_id,
            dispatch ? dispatch->name() : "<none>");

        if(dispatch) {
            // NOTE: For mute slots, creating a new channel will essentially leak memory, since no
            // response will ever be sent back, therefore the channel will never be revoked at all.
            mapping.insert({channel_id, std::make_shared<channel_t>(dispatch, downstream)});
        }

        return downstream;
    });
}

// Channel I/O

void
session_t::pull() {
#if defined(__clang__)
    if(const auto ptr = std::atomic_load(&transport)) {
#else
    if(const auto ptr = *transport.synchronize()) {
#endif
        // Use dispatch() instead of a direct call for thread safety.
        ptr->socket->get_io_service().dispatch(std::bind(&pull_action_t::operator(),
            std::make_shared<pull_action_t>(shared_from_this()),
            ptr
        ));
    } else {
        throw std::system_error(error::not_connected);
    }
}

void
session_t::push(encoder_t::message_type&& message) {
#if defined(__clang__)
    if(const auto ptr = std::atomic_load(&transport)) {
#else
    if(const auto ptr = *transport.synchronize()) {
#endif
        // Use dispatch() instead of a direct call for thread safety.
        ptr->socket->get_io_service().dispatch(trace_t::bind(&push_action_t::operator(),
            std::make_shared<push_action_t>(std::move(message), shared_from_this()),
            ptr
        ));
    } else {
        throw std::system_error(error::not_connected);
    }
}

void
session_t::detach(const std::error_code& ec) {
#if defined(__clang__)
    if(auto swapped = std::atomic_exchange(&transport, std::shared_ptr<transport_type>())) {
#else
    if(auto swapped = std::move(*transport.synchronize())) {
#endif
        swapped = nullptr;
        COCAINE_LOG_DEBUG(log, "detached session from the transport");
    } else {
        COCAINE_LOG_WARNING(log, "ignoring detach request for session");
        return;
    }

    channels.apply([&](channel_map_t& mapping) {
        if(mapping.empty()) {
            return;
        } else {
            COCAINE_LOG_DEBUG(log, "discarding {:d} channel dispatch(es)", mapping.size());
        }

        for(auto it = mapping.begin(); it != mapping.end(); ++it) {
            if(it->second->dispatch) it->second->dispatch->discard(ec);
        }

        mapping.clear();
    });
}

// Information

std::map<uint64_t, std::string>
session_t::active_channels() const {
    return channels.apply([](const channel_map_t& mapping) -> std::map<uint64_t, std::string> {
        std::map<uint64_t, std::string> result;

        for(auto it = mapping.begin(); it != mapping.end(); ++it) {
            result[it->first] = it->second->dispatch ? it->second->dispatch->name() : "<none>";
        }

        return result;
    });
}

std::size_t
session_t::memory_pressure() const {
#if defined(__clang__)
    if(const auto ptr = std::atomic_load(&transport)) {
#else
    if(const auto ptr = *transport.synchronize()) {
#endif
        return ptr->reader->pressure() + ptr->writer->pressure();
    } else {
        return 0;
    }
}

std::string
session_t::name() const {
    return prototype ? prototype->name() : "<none>";
}

session_t::endpoint_type
session_t::remote_endpoint() const {
    endpoint_type endpoint;

#if defined(__clang__)
    if(const auto ptr = std::atomic_load(&transport)) {
#else
    if(const auto ptr = *transport.synchronize()) {
#endif
        try {
            endpoint = ptr->socket->remote_endpoint();
        } catch(const std::system_error& e) {
            // Ignore.
        }
    }

    return endpoint;
}

namespace cocaine {

template<class Protocol>
session<Protocol>::session(std::unique_ptr<logging::logger_t> log, std::unique_ptr<transport_type> transport, const dispatch_ptr_t& prototype):
    session_t(std::move(log),
              std::make_unique<io::transport<generic::stream_protocol>>(std::move(*transport)),
              std::move(prototype))
{ }

template<>
typename session<ip::tcp>::endpoint_type
session<ip::tcp>::remote_endpoint() const {
    const auto source = session_t::remote_endpoint();

    BOOST_ASSERT(source.protocol() == ip::tcp::v4() || source.protocol() == ip::tcp::v6());

    auto transformed = ip::tcp::endpoint();

    transformed.resize(source.size());
    std::memcpy(transformed.data(), source.data(), source.size());

    return transformed;
}

template
class session<ip::tcp>;

template
class session<local::stream_protocol>;

} // namespace cocaine
