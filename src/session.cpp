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

#include "cocaine/rpc/asio/channel.hpp"

#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/upstream.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;

using namespace cocaine;
using namespace cocaine::io;

// Session internals

class session_t::channel_t {
    friend class session_t;

    dispatch_ptr_t dispatch;
    upstream_ptr_t upstream;

public:
    channel_t(const dispatch_ptr_t& dispatch_, const upstream_ptr_t& upstream_):
        dispatch(dispatch_),
        upstream(upstream_)
    { }

    void
    process(const decoder_t::message_type& message);
};

void
session_t::channel_t::process(const decoder_t::message_type& message) {
    if(!dispatch) {
        throw cocaine::error_t("no dispatch has been assigned");
    }

    if((dispatch = dispatch->process(message, upstream).get_value_or(dispatch)) == nullptr) {
        // NOTE: If the client has sent us the last message according to our dispatch graph, then
        // revoke the channel.
        upstream->drop();
        upstream = nullptr;
    }
}

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
    operator()();

private:
    void
    finalize(const boost::system::error_code& ec);
};

void
session_t::pull_action_t::operator()() {
    // TODO: Locking.
    if(!session->ptr) {
        return;
    }

    session->ptr->reader->read(message, std::bind(&pull_action_t::finalize,
        shared_from_this(),
        std::placeholders::_1
    ));
}

void
session_t::pull_action_t::finalize(const boost::system::error_code& ec) {
    if(ec) {
        return session->signals.shutdown(ec);
    }

    // TODO: Locking.
    if(!session->ptr) {
        return;
    }

    try {
        session->invoke(message);
    } catch(...) {
        // TODO: Show the actual error message. The best sink would probably be the prototype's log,
        // but for client sessions it's not available, so think about it a bit more.
        // NOTE: This happens only when the underlying slot has miserably failed to handle service's
        // exceptions. In such case, the client is disconnected to prevent any further damage.
        return session->signals.shutdown(error::uncaught_error);
    }

    operator()();
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
    operator()();

private:
    void
    finalize(const boost::system::error_code& ec);
};

void
session_t::push_action_t::operator()() {
    // TODO: Locking.
    if(!session->ptr) {
        return;
    }

    session->ptr->writer->write(message, std::bind(&push_action_t::finalize,
        shared_from_this(),
        std::placeholders::_1
    ));
}

void
session_t::push_action_t::finalize(const boost::system::error_code& ec) {
    if(ec) {
        return session->signals.shutdown(ec);
    }
}

class session_t::discard_action_t {
    synchronized<channel_map_t>& channels;

public:
    discard_action_t(synchronized<channel_map_t>& channels_):
        channels(channels_)
    { }

    void
    operator()(const boost::system::error_code& ec);
};

void
session_t::discard_action_t::operator()(const boost::system::error_code& ec) {
    auto ptr = channels.synchronize();

    for(auto it = ptr->begin(); it != ptr->end(); ++it) {
        if(it->second->dispatch) it->second->dispatch->discard(ec);
    }

    ptr->clear();
}

// Session

session_t::session_t(std::unique_ptr<channel<tcp>> ptr_, const dispatch_ptr_t& prototype_):
    ptr(std::move(ptr_)),
    endpoint(ptr->socket->remote_endpoint()),
    prototype(prototype_),
    max_channel_id(0)
{
    signals.shutdown.connect(0, discard_action_t(channels));
}

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

    channel->process(message);
}

upstream_ptr_t
session_t::inject(const dispatch_ptr_t& dispatch) {
    auto ptr = channels.synchronize();

    const auto channel_id = ++max_channel_id;
    const auto upstream = std::make_shared<basic_upstream_t>(shared_from_this(), channel_id);

    if(dispatch) {
        ptr->insert({channel_id, std::make_shared<channel_t>(dispatch, upstream)});
    }

    return upstream;
}

void
session_t::revoke(uint64_t channel_id) {
    channels->erase(channel_id);
}

void
session_t::detach() {
    {
        std::lock_guard<std::mutex> guard(mutex);
        ptr = nullptr;
    }

    // Detach all the signal handlers, because the session will be in detached state and triggering
    // more signals will result in an undefined behavior.
    signals.shutdown.disconnect_all_slots();
}

// Channel I/O

void
session_t::pull() {
    std::lock_guard<std::mutex> guard(mutex);

    if(!ptr) return;

    // Use dispatch() instead of a direct call for thread safety.
    ptr->socket->get_io_service().dispatch(std::bind(&pull_action_t::operator(),
        std::make_shared<pull_action_t>(shared_from_this())
    ));
}

void
session_t::push(encoder_t::message_type&& message) {
    std::lock_guard<std::mutex> guard(mutex);

    if(!ptr) return;

    // Use dispatch() instead of a direct call for thread safety.
    ptr->socket->get_io_service().dispatch(std::bind(&push_action_t::operator(),
        std::make_shared<push_action_t>(std::move(message), shared_from_this())
    ));
}

// Information

std::map<uint64_t, std::string>
session_t::active_channels() const {
    std::map<uint64_t, std::string> result;

    auto ptr = channels.synchronize();

    for(auto it = ptr->begin(); it != ptr->end(); ++it) {
        result[it->first] = it->second->dispatch ? it->second->dispatch->name() : "<unassigned>";
    }

    return result;
}

std::tuple<size_t, size_t>
session_t::memory_pressure() const {
    std::lock_guard<std::mutex> guard(mutex);

    if(ptr) {
        return { ptr->reader->pressure(), ptr->writer->pressure() };
    } else {
        return { 0, 0 };
    }
}

std::string
session_t::name() const {
    return prototype ? prototype->name() : "<unassigned>";
}

tcp::endpoint
session_t::remote_endpoint() const {
    return endpoint;
}
