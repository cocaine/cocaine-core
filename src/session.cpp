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

#include "cocaine/rpc/session.hpp"

#include "cocaine/dispatch.hpp"

#include "cocaine/rpc/upstream.hpp"

using namespace cocaine;
using namespace cocaine::io;

class session_t::channel_t {
    std::shared_ptr<dispatch_t> dispatch;
    std::shared_ptr<upstream_t> upstream;

public:
    channel_t(const std::shared_ptr<dispatch_t>& dispatch_, const std::shared_ptr<upstream_t>& upstream_):
        dispatch(dispatch_),
        upstream(upstream_)
    { }

    void
    invoke(const message_t& message) {
        if(!dispatch) {
            // TODO: COCAINE-82 adds a 'client' error category.
            throw cocaine::error_t("channel has been deactivated");
        }

        dispatch = dispatch->invoke(message, upstream);
    }
};

void
session_t::invoke(const message_t& message) {
    std::shared_ptr<channel_t> channel;

    {
        channel_map_t::const_iterator lb, ub;
        channel_map_t::key_type index = message.band();

        auto locked = channels.synchronize();

        std::tie(lb, ub) = locked->equal_range(index);

        if(lb == ub) {
            std::tie(lb, std::ignore) = locked->insert({index, std::make_shared<channel_t>(
                prototype,
                std::make_shared<upstream_t>(shared_from_this(), index)
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

void
session_t::revoke() {
    std::lock_guard<std::mutex> guard(mutex);

    // NOTE: This invalidates and closes the internal connection pointer and destroys the protocol
    // dispatches, but the session itself might still be accessible via upstreams in other threads.
    // And that's okay, since it has no resources associated with it anymore.

    channels->clear();
    ptr.reset();
}

void
session_t::detach(uint64_t index) {
    channels->erase(index);
}
