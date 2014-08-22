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

    std::shared_ptr<const basic_dispatch_t> dispatch;
    std::shared_ptr<basic_upstream_t> upstream;

public:
    channel_t(const std::shared_ptr<const basic_dispatch_t>& dispatch_, const std::shared_ptr<basic_upstream_t>& upstream_):
        dispatch(dispatch_),
        upstream(upstream_)
    { }

    void
    invoke(const decoder_t::message_type& message);
};

void
session_t::channel_t::invoke(const decoder_t::message_type& message) {
    if(!dispatch) {
        throw cocaine::error_t("no dispatch has been assigned");
    }

    if((dispatch = dispatch->call(message, upstream).get_value_or(dispatch)) == nullptr) {
        // NOTE: If the client has sent us the last message according to the dispatch graph, then
        // revoke the channel.
        upstream->drop();
    }
}

// Session

session_t::session_t(std::unique_ptr<channel<tcp>> ptr_, const std::shared_ptr<const basic_dispatch_t>& prototype_):
    ptr(std::move(ptr_)),
    endpoint(ptr->remote_endpoint()),
    prototype(prototype_),
    max_channel(0)
{ }

void
session_t::invoke(const decoder_t::message_type& message) {
    channel_map_t::const_iterator lb, ub;
    const channel_map_t::key_type index = message.span();

    std::shared_ptr<channel_t> channel;

    {
        auto ptr = channels.synchronize();

        std::tie(lb, ub) = ptr->equal_range(index);

        if(lb == ub) {
            if(index <= max_channel) {
                return;
            }

            // NOTE: Checking whether channel number is always higher than the previous channel number
            // is similar to an infinite TIME_WAIT timeout for TCP sockets. It might be not the best
            // aproach, but since we have 2^64 possible channels, unlike 2^16 ports for sockets, it is
            // fit to avoid stray messages.

            max_channel = index;

            std::tie(lb, std::ignore) = ptr->insert({index, std::make_shared<channel_t>(
                prototype,
                std::make_shared<basic_upstream_t>(shared_from_this(), index)
            )});
        }

        // NOTE: The virtual channel pointer is copied here so that if the slot decides to close the
        // virtual channel, it won't destroy it inside the channel_t::invoke(). Instead, it will be
        // destroyed when this function scope is exited, liberating us from thinking of some voodoo
        // magic to handle it.

        channel = lb->second;
    }

    channel->invoke(message);
}

std::shared_ptr<basic_upstream_t>
session_t::inject(const std::shared_ptr<const basic_dispatch_t>& dispatch) {
    auto ptr = channels.synchronize();

    const auto index = ++max_channel;
    const auto upstream = std::make_shared<basic_upstream_t>(shared_from_this(), index);

    // TODO: Think about skipping dispatch registration in case of fire-and-forget service events.
    ptr->insert({index, std::make_shared<channel_t>(dispatch, upstream)});

    return upstream;
}

void
session_t::revoke(uint64_t index) {
    channels->erase(index);
}

void
session_t::detach() {
    {
        std::lock_guard<std::mutex> guard(mutex);
        ptr.reset();
    }

    channels->clear();
}

// I/O

class session_t::pull_action_t:
    public std::enable_shared_from_this<pull_action_t>
{
    std::shared_ptr<session_t> session;
    decoder_t::message_type message;

public:
    pull_action_t(const std::shared_ptr<session_t>& session_):
        session(session_)
    { }

    // TODO: Locking.

    void
    operator()() {
        if(!session->ptr) {
            return;
        }

        session->ptr->reader->read(message,
            std::bind(&pull_action_t::finalize, shared_from_this(), std::placeholders::_1)
        );
    }

private:
    void
    finalize(const boost::system::error_code& ec) {
        if(ec) {
            if(session->ptr) {
                session->signals.shutdown(ec);
            }

            return;
        }

        try {
            session->invoke(message);
        } catch(const cocaine::error_t& e) {
            if(session->ptr) {
                session->signals.shutdown(error::uncaught_error);
            }

            return;
        }

        operator()();
    }
};

void
session_t::pull() {
    std::lock_guard<std::mutex> guard(mutex);

    if(!ptr) return;

    ptr->socket->get_io_service().dispatch(
        // Use dispatch() instead of a direct call for thread safety.
        std::bind(&pull_action_t::operator(), std::make_shared<pull_action_t>(shared_from_this())
    ));
}

class session_t::push_action_t:
    public enable_shared_from_this<push_action_t>
{
    std::shared_ptr<session_t> session;
    encoder_t::message_type message;

public:
    push_action_t(const std::shared_ptr<session_t>& session_, encoder_t::message_type&& message):
        session(session_),
        message(std::move(message))
    { }

    // TODO: Locking.

    void
    operator()() {
        if(!session->ptr) {
            return;
        }

        session->ptr->writer->write(message,
            std::bind(&push_action_t::finalize, shared_from_this(), std::placeholders::_1)
        );
    }

private:
    void
    finalize(const boost::system::error_code& ec) {
        if(ec && session->ptr) {
            session->signals.shutdown(ec);
        }
    }
};

void
session_t::push(encoder_t::message_type&& message) {
    std::lock_guard<std::mutex> guard(mutex);

    if(!ptr) return;

    ptr->socket->get_io_service().dispatch(
        // Use dispatch() instead of a direct call for thread safety.
        std::bind(&push_action_t::operator(), std::make_shared<push_action_t>(shared_from_this(), std::move(message))
    ));
}

tcp::endpoint
session_t::remote_endpoint() const {
    return endpoint;
}

std::string
session_t::name() const {
    if(prototype) {
        return prototype->name();
    } else {
        return "<unassigned>";
    }
}

std::map<uint64_t, std::string>
session_t::active_channels() const {
    std::map<uint64_t, std::string> result;

    auto ptr = channels.synchronize();

    for(auto it = ptr->begin(); it != ptr->end(); ++it) {
        result[it->first] = it->second->dispatch ? it->second->dispatch->name() : "<unassigned>";
    }

    return result;
}
