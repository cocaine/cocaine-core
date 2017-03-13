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
        data(std::make_shared<synchronized<data_t>>())
    { }

    template<class... Args>
    typename std::enable_if<
        std::is_constructible<T, Args...>::value,
        std::error_code
    >::type
    write(hpack::header_storage_t headers, Args&&... args) {
        auto d = data->synchronize();
        if (d->state == state_t::closed) {
            return make_error_code(error::protocol_errors::closed_upstream);
        }

        return d->outbox.template append<chunk_type>(std::move(headers), std::forward<Args>(args)...);
    }

    template<class... Args>
    typename std::enable_if<
        std::is_constructible<T, Args...>::value,
        std::error_code
    >::type
    write(Args&&... args) {
        return write({}, std::forward<Args>(args)...);
    }

    std::error_code
    abort(hpack::header_storage_t headers, const std::error_code& ec, const std::string& reason) {
        return data->apply([&](data_t& data) {
            if(utility::exchange(data.state, state_t::closed) == state_t::closed) {
                return make_error_code(error::protocol_errors::closed_upstream);
            }

            return data.outbox.template append<error_type>(std::move(headers), ec, reason);
        });
    }

    std::error_code
    abort(const std::error_code& ec, const std::string& reason) {
        return abort({}, ec, reason);
    }

    std::error_code
    close(hpack::header_storage_t headers) {
        return data->apply([&](data_t& data) {
            if (utility::exchange(data.state, state_t::closed) == state_t::closed) {
                return make_error_code(error::protocol_errors::closed_upstream);
            }

            return data.outbox.template append<choke_type>(std::move(headers));
        });
    }

    std::error_code
    close() {
        return close({});
    }

    template<class UpstreamType>
    void
    attach(UpstreamType&& upstream) {
        data->synchronize()->outbox.attach(std::move(upstream));
    }

private:
    enum class state_t {
        open,
        closed
    };

    struct data_t {
        data_t(): state(state_t::open) {}

        state_t state;
        queue_type outbox;
    };

    const std::shared_ptr<synchronized<data_t>> data;
};

} // namespace cocaine

#endif
