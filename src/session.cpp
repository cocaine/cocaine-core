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

#include <asio/ip/tcp.hpp>
#include <asio/local/stream_protocol.hpp>

#include <boost/lexical_cast.hpp>

#include <blackhole/logger.hpp>

#include <metrics/accumulator/decaying/exponentially.hpp>
#include <metrics/accumulator/snapshot/weighted.hpp>
#include <metrics/meter.hpp>
#include <metrics/registry.hpp>
#include <metrics/timer.hpp>

#include "cocaine/hpack/static_table.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/rpc/asio/transport.hpp"
#include "cocaine/rpc/basic_dispatch.hpp"
#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/upstream.hpp"

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

            // TODO: it seems that we can move it into and process message everywhere by value
            // This can help to avoid unnecsesarry headers copy
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

class load_watcher_t {
    metrics::shared_metric<std::atomic<std::int64_t>> load;

public:
    load_watcher_t(metrics::shared_metric<std::atomic<std::int64_t>> load) :
        load(std::move(load))
    {
        this->load->fetch_add(1);
    }

    ~load_watcher_t() {
        this->load->fetch_add(-1);
    }
};

struct session_t::channel_t {
    dispatch_ptr_t dispatch;
    upstream_ptr_t upstream;
    std::shared_ptr<load_watcher_t> load;
    std::shared_ptr<metrics::timer_t::context_t> context;
    boost::optional<trace_t> trace;
};

namespace {

auto
dispatch_name(const dispatch_ptr_t& dispatch) -> std::string {
    if (dispatch) {
        return dispatch->name();
    } else {
        return "<none>";
    }
}

} // namespace

// Session

struct session_t::metrics_t {
    using meter_type = metrics::shared_metric<metrics::meter_t>;
    using timer_type = metrics::shared_metric<metrics::timer<metrics::accumulator::decaying::exponentially_t>>;

    /// RPS counter.
    meter_type summary;

    /// Load gauge.
    metrics::shared_metric<std::atomic<std::int64_t>> load;

    /// Timers per slot.
    std::map<
        int,
        timer_type
    > timers;

    metrics_t(metrics::registry_t& metrics_hub, session_t& session) :
        summary(metrics_hub.meter(cocaine::format("{}.meter.summary", session.name()))),
        load{
            metrics_hub.counter<std::int64_t>(cocaine::format("{}.load", session.name())),
        }
    {
        for (auto& item : session.prototype->root()) {
            auto id = std::get<0>(item);
            auto& name = std::get<0>(std::get<1>(item));

            auto metric_name = cocaine::format("{}.timer[{}]", session.prototype->name(), name);
            timers.emplace(
                id,
                metrics_hub.timer<metrics::accumulator::decaying::exponentially_t>(metric_name)
            );
        }
    }

    auto
    timer(int id) const -> boost::optional<timer_type> {
        auto it = timers.find(id);
        if (it == std::end(timers)) {
            return boost::none;
        }

        return it->second;
    }
};

struct metered_upstream_t : public basic_upstream_t {
    std::shared_ptr<load_watcher_t> load;
    std::shared_ptr<metrics::timer_t::context_t> timer;

    metered_upstream_t(const std::shared_ptr<session_t>& session,
                       uint64_t channel_id,
                       std::shared_ptr<load_watcher_t> load,
                       std::shared_ptr<metrics::timer_t::context_t> timer)
        : basic_upstream_t(session, channel_id), load(std::move(load)), timer(std::move(timer)) {}
};

session_t::session_t(std::unique_ptr<logging::logger_t> log_,
                     metrics::registry_t& metrics_hub,
                     std::unique_ptr<transport_type> transport_,
                     const dispatch_ptr_t& prototype_)
    : log(std::move(log_)),
      transport(std::shared_ptr<transport_type>(std::move(transport_))),
      prototype(prototype_),
      max_channel_id(0)
{
    if (prototype) {
        metrics = std::make_unique<metrics_t>(metrics_hub, *this);
    }

    auto dispatch = std::make_shared<cocaine::dispatch<io::control_tag>>("session");

    dispatch->on<io::control::ping>([&] {
        return;
    });
    dispatch->on<io::control::revoke>([&](std::uint64_t id, std::error_code ec) {
        revoke(id, ec);
    });

    service_dispatch = std::move(dispatch);
}

session_t::~session_t() = default;

// Operations

void
session_t::handle(const decoder_t::message_type& message) {
    const channel_map_t::key_type channel_id = message.span();
    boost::optional<trace_t> trace;

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

            trace = extract_trace(message);

            auto timer = std::make_shared<metrics::timer_t::context_t>(metrics->timers.at(message.type())->context());
            auto watcher = std::make_shared<load_watcher_t>(metrics->load);

            std::tie(lb, std::ignore) = mapping.insert({channel_id, std::make_shared<channel_t>(
                channel_t{
                    select_dispatch(message),
                    std::make_shared<metered_upstream_t>(
                        shared_from_this(),
                        channel_id,
                        watcher,
                        timer
                    ),
                    watcher,
                    timer,
                    trace
                }
            )});
            metrics->summary->mark();

            max_channel_id = channel_id;
        } else {
            trace = lb->second->trace;
        }

        // NOTE: The virtual channel pointer is copied here to avoid data races.
        return lb->second;
    });

    if(!channel->dispatch) {
        throw std::system_error(error::unbound_dispatch);
    }

    trace_t::restore_scope_t trace_scope(trace);

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
        if(!channel.unique()) {
            revoke(channel_id);
        }
    }
}

auto
session_t::extract_trace(const io::decoder_t::message_type& message) const -> boost::optional<trace_t> {
    auto& headers = message.headers();

    auto trace = hpack::header::find_first<hpack::headers::trace_id<>>(headers);
    auto span = hpack::header::find_first<hpack::headers::span_id<>>(headers);
    auto parent = hpack::header::find_first<hpack::headers::parent_id<>>(headers);
    if (trace && span && parent) {
        bool verbose = false;
        if (auto header = hpack::header::find_first(headers, "trace_bit")) {
            verbose = boost::lexical_cast<bool>(header->value());
        }

        return trace_t(
            hpack::header::unpack<std::uint64_t>(trace->value()),
            hpack::header::unpack<std::uint64_t>(span->value()),
            hpack::header::unpack<std::uint64_t>(parent->value()),
            verbose,
            std::get<0>(prototype->root().at(message.type()))
        );
    }

    return boost::none;
}

auto
session_t::select_dispatch(const io::decoder_t::message_type& message) const -> io::dispatch_ptr_t {
    // Hack to be able to properly dispatch control messages.
    if(message.type() < prototype->root().size()) {
        return prototype;
    } else {
        return service_dispatch;
    }
}

void
session_t::revoke(uint64_t id) {
    revoke(id, std::error_code());
}

void
session_t::revoke(uint64_t id, std::error_code ec) {
    channels.apply([&](channel_map_t& mapping) {
        auto it = mapping.find(id);

        if(it == mapping.end()) {
            COCAINE_LOG_WARNING(log, "ignoring revoke request for channel {:d}", id);
            return;
        }

        if(it->second->dispatch) {
            COCAINE_LOG_ERROR(log, "revoking channel {:d}, dispatch: '{}'", id,
                it->second->dispatch->name());
            it->second->dispatch->discard(ec);
        } else {
            COCAINE_LOG_DEBUG(log, "revoking channel {:d}", id);
        }

        mapping.erase(it);
    });
}

upstream_ptr_t
session_t::fork(const dispatch_ptr_t& dispatch) {
    return channels.apply([&](channel_map_t& mapping) -> upstream_ptr_t {
        const auto channel_id = ++max_channel_id;
        auto trace = trace_t::current();
        trace.push(dispatch_name(dispatch));
        const auto downstream = std::make_shared<basic_upstream_t>(shared_from_this(), channel_id);

        COCAINE_LOG_DEBUG(log, "forking new channel {:d}, dispatch: '{}'", channel_id, dispatch_name(dispatch));

        if(dispatch) {
            // NOTE: For mute slots, creating a new channel will essentially leak memory, since no
            // response will ever be sent back, therefore the channel will never be revoked at all.
            mapping.insert({channel_id, std::make_shared<channel_t>(
                channel_t{
                    dispatch,
                    downstream,
                    nullptr,
                    nullptr,
                    trace
                }
            )});
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
        // Use post() instead of a direct call for thread safety.
        // We can not use dispatch here to prevent channel reordering.
        ptr->socket->get_io_service().post(trace_t::bind(&push_action_t::operator(),
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
            result[it->first] = dispatch_name(it->second->dispatch);
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
    return dispatch_name(prototype);
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

template <class Protocol>
session<Protocol>::session(std::unique_ptr<logging::logger_t> log,
                           metrics::registry_t& metrics_hub,
                           std::unique_ptr<transport_type> transport,
                           const dispatch_ptr_t& prototype)
    : session_t(std::move(log),
                metrics_hub,
                std::make_unique<io::transport<generic::stream_protocol>>(std::move(*transport)),
                std::move(prototype)) {}

template<>
typename session<ip::tcp>::endpoint_type
session<ip::tcp>::remote_endpoint() const {
    const auto source = session_t::remote_endpoint();

    // TODO: Uncomment. In our production we receive protocol == 0.
    // BOOST_ASSERT(source.protocol() == ip::tcp::v4() || source.protocol() == ip::tcp::v6());

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
