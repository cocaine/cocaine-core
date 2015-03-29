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

#include "cocaine/logging.hpp"

#include "cocaine/rpc/asio/channel.hpp"

#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/upstream.hpp"

using namespace asio;
using namespace asio::ip;

using namespace cocaine;
using namespace cocaine::io;

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
    operator()(const std::shared_ptr<channel<tcp>> ptr);

private:
    void
    finalize(const std::error_code& ec);
};

void
session_t::pull_action_t::operator()(const std::shared_ptr<channel<tcp>> ptr) {
    ptr->reader->read(message, std::bind(&pull_action_t::finalize,
        shared_from_this(),
        std::placeholders::_1
    ));
}

void
session_t::pull_action_t::finalize(const std::error_code& ec) {
    if(ec) {
        if(ec != asio::error::eof) {
            COCAINE_LOG_ERROR(session->log, "client disconnected: [%d] %s", ec.value(), ec.message());
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
            session->invoke(message);
        } catch(const std::exception& e) {
            COCAINE_LOG_ERROR(session->log, "uncaught invocation exception - %s", e.what());

            // NOTE: This happens only when the underlying slot has miserably failed to handle its
            // exceptions. In such case, the client is disconnected to prevent any further damage.
            return session->detach(error::uncaught_error);
        }

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
    operator()(const std::shared_ptr<channel<tcp>> ptr);

private:
    void
    finalize(const std::error_code& ec);
};

void
session_t::push_action_t::operator()(const std::shared_ptr<channel<tcp>> ptr) {
    ptr->writer->write(message, std::bind(&push_action_t::finalize,
        shared_from_this(),
        std::placeholders::_1
    ));
}

void
session_t::push_action_t::finalize(const std::error_code& ec) {
    if(ec.value() == 0) return;

    if(ec != asio::error::eof) {
        COCAINE_LOG_ERROR(session->log, "client disconnected: [%d] %s", ec.value(), ec.message());
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

session_t::session_t(std::unique_ptr<logging::log_t> log_,
                     std::unique_ptr<channel<tcp>> transport_, const dispatch_ptr_t& prototype_):
    log(std::move(log_)),
    transport(std::shared_ptr<channel<tcp>>(std::move(transport_))),
    prototype(prototype_),
    max_channel_id(0)
{ }

// Operations

void
session_t::invoke(const decoder_t::message_type& message) {
    channel_map_t::const_iterator lb, ub;
    channel_map_t::key_type channel_id = message.span();

    std::shared_ptr<channel_t> channel;

    {
        auto ptr = channels.synchronize();

        std::tie(lb, ub) = ptr->equal_range(channel_id);

        if(lb == ub) {
            if(channel_id <= max_channel_id) {
                COCAINE_LOG_WARNING(log, "ignoring invocation due to invalid channel id");

                // NOTE: Checking whether channel number is always higher than the previous channel
                // number is similar to an infinite TIME_WAIT timeout for TCP sockets. It might be
                // not the best approach, but since we have 2^64 possible channels, and it is a lot
                // more than 2^16 ports for sockets, it is fit to avoid stray messages.
                return;
            }

            max_channel_id = channel_id;

            std::tie(lb, std::ignore) = ptr->insert({channel_id, std::make_shared<channel_t>(
                prototype,
                std::make_shared<basic_upstream_t>(shared_from_this(), channel_id)
            )});
        }

        // NOTE: The virtual channel pointer is copied here so that if the slot decides to close the
        // virtual channel, it won't destroy it inside the channel_t::process(). Instead, it will be
        // destroyed when this function scope is exited, liberating us from thinking of some voodoo
        // workaround magic.
        channel = lb->second;
    }

    if(!channel->dispatch) {
        throw cocaine::error_t("no dispatch has been assigned");
    }

    COCAINE_LOG_DEBUG(log, "processing invocation type %llu: '%s' in channel %llu, dispatch: '%s'",
        message.type(), std::get<0>(channel->dispatch->root().at(message.type())), channel_id,
        channel->dispatch->name());

    if((channel->dispatch = channel->dispatch->process(message, channel->upstream)
        .get_value_or(channel->dispatch)) == nullptr)
    {
        // NOTE: If the client has sent us the last message according to our dispatch graph, revoke
        // the channel. No-op if the channel is no longer in the mapping (e.g., was discarded).
        if(!channel.unique()) revoke(channel_id);
    }
}

upstream_ptr_t
session_t::inject(const dispatch_ptr_t& dispatch) {
    auto ptr = channels.synchronize();

    const auto channel_id = ++max_channel_id;
    const auto downstream = std::make_shared<basic_upstream_t>(shared_from_this(), channel_id);

    COCAINE_LOG_DEBUG(log, "processing injection in channel %llu, dispatch: '%s'", channel_id,
        dispatch ? dispatch->name() : "<none>");

    if(dispatch) {
        ptr->insert({channel_id, std::make_shared<channel_t>(dispatch, downstream)});
    }

    return downstream;
}

void
session_t::revoke(uint64_t channel_id) {
    channels.apply([=](channel_map_t& mapping) {
        auto it = mapping.find(channel_id);

        // NOTE: Not sure if that can ever happen, but that's why people use asserts, right?
        BOOST_ASSERT(it != mapping.end());

        COCAINE_LOG_DEBUG(log, "processing revocation of channel %llu, dispatch: '%s'", channel_id,
            it->second->dispatch ? it->second->dispatch->name() : "<none>");

        mapping.erase(it);
    });
}

void
session_t::detach(const std::error_code& ec) {
    COCAINE_LOG_DEBUG(log, "detaching session from the transport");

    channels.apply([=](channel_map_t& mapping) {
        if(mapping.empty()) {
            return;
        } else {
            COCAINE_LOG_DEBUG(log, "discarding %llu channel dispatch(es)", mapping.size());
        }

        for(auto it = mapping.begin(); it != mapping.end(); ++it) {
            if(it->second->dispatch) it->second->dispatch->discard(ec);
        }

        mapping.clear();
    });

#if defined(__clang__)
    std::atomic_store(&transport, std::shared_ptr<channel<tcp>>());
#else
    *transport.synchronize() = nullptr;
#endif
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
        throw cocaine::error_t("session is not connected");
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
        ptr->socket->get_io_service().dispatch(std::bind(&push_action_t::operator(),
            std::make_shared<push_action_t>(std::move(message), shared_from_this()),
            ptr
        ));
    } else {
        throw cocaine::error_t("session is not connected");
    }
}

// Information

std::map<uint64_t, std::string>
session_t::active_channels() const {
    std::map<uint64_t, std::string> result;

    auto ptr = channels.synchronize();

    for(auto it = ptr->begin(); it != ptr->end(); ++it) {
        result[it->first] = it->second->dispatch ? it->second->dispatch->name() : "<none>";
    }

    return result;
}

size_t
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

tcp::endpoint
session_t::remote_endpoint() const {
    tcp::endpoint endpoint;

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
