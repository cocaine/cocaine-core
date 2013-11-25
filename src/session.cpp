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

class session_t::downstream_t {
    // Active protocol for this downstream.
    std::shared_ptr<dispatch_t> dispatch;

    // As of now, all clients are considered using the streaming protocol template, and it means that
    // upstreams don't change when the downstream protocol is switched over.
    std::shared_ptr<upstream_t> upstream;

public:
    downstream_t(const std::shared_ptr<dispatch_t>& dispatch_, const std::shared_ptr<upstream_t>& upstream_):
        dispatch(dispatch_),
        upstream(upstream_)
    { }

    void
    invoke(const message_t& message) {
        if(!dispatch) {
            // TODO: COCAINE-82 adds a 'client' error category.
            throw cocaine::error_t("downstream has been deactivated");
        }

        dispatch = dispatch->invoke(message, upstream);
    }
};

void
session_t::invoke(const message_t& message) {
    std::shared_ptr<downstream_t> downstream;

    {
        std::lock_guard<std::mutex> guard(mutex);

        auto index = message.band();
        auto it    = downstreams.find(index);

        if(it == downstreams.end()) {
            std::tie(it, std::ignore) = downstreams.insert({ index, std::make_shared<downstream_t>(
                prototype,
                std::make_shared<upstream_t>(shared_from_this(), index)
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
session_t::revoke() {
    std::lock_guard<std::mutex> guard(mutex);

    // This closes all the downstreams.
    downstreams.clear();

    // NOTE: This invalidates and closes the internal channel pointer, but the session itself
    // might still be accessible via upstreams in other threads, but that's okay.
    ptr.reset();
}

void
session_t::detach(uint64_t index) {
    downstreams.erase(index);
}
