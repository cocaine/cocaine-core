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

#ifndef COCAINE_IO_STREAMED_SLOT_HPP
#define COCAINE_IO_STREAMED_SLOT_HPP

#include "cocaine/rpc/slot/deferred.hpp"
#include "cocaine/utility/exchange.hpp"

namespace cocaine {

template<class T>
struct streamed {
    typedef typename aux::reconstruct<T>::type type;

    typedef io::message_queue<io::streaming_tag<type>> queue_type;
    typedef io::streaming<type> protocol;

    typedef typename protocol::chunk chunk_type;
    typedef typename protocol::error error_type;
    typedef typename protocol::choke choke_type;

    streamed():
        state(state_t::open),
        outbox(new synchronized<queue_type>())
    { }

    template<class... Args>
    typename std::enable_if<
        std::is_constructible<T, Args...>::value,
        streamed&
    >::type
    write(hpack::header_storage_t headers, Args&&... args) {
        outbox->apply([&](queue_type& queue) {
            if (state == state_t::closed) {
                throw std::invalid_argument("queue is closed");
            }

            queue.template append<chunk_type>(std::move(headers), std::forward<Args>(args)...);
        });

        return *this;
    }

    template<class... Args>
    typename std::enable_if<
        std::is_constructible<T, Args...>::value,
        streamed&
    >::type
    write(Args&&... args) {
        return write({}, std::forward<Args>(args)...);
    }

    streamed&
    abort(hpack::header_storage_t headers, const std::error_code& ec, const std::string& reason) {
        outbox->apply([&](queue_type& queue) {
            if (utility::exchange(state, state_t::closed) == state_t::closed) {
                throw std::invalid_argument("queue is already closed");
            }

            queue.template append<error_type>(std::move(headers), ec, reason);
        });

        return *this;
    }

    streamed&
    abort(const std::error_code& ec, const std::string& reason) {
        return abort({}, ec, reason);
    }

    streamed&
    close(hpack::header_storage_t headers) {
        outbox->apply([&](queue_type& queue) {
            if (utility::exchange(state, state_t::closed) == state_t::closed) {
                throw std::invalid_argument("queue is already closed");
            }

            queue.template append<choke_type>(std::move(headers));
        });

        return *this;
    }

    streamed&
    close() {
        return close({});
    }

    template<class UpstreamType>
    void
    attach(UpstreamType&& upstream) {
        outbox->synchronize()->attach(std::move(upstream));
    }

private:
    enum class state_t {
        open,
        closed
    };

    state_t state;
    const std::shared_ptr<synchronized<queue_type>> outbox;
};

} // namespace cocaine

#endif
