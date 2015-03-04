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

namespace cocaine {

template<class T>
struct streamed {
    typedef typename aux::reconstruct<T>::type type;

    typedef io::message_queue<io::streaming_tag<type>> queue_type;
    typedef io::streaming<type> protocol;

    template<template<class> class, class, class> friend struct io::deferred_slot;

    streamed():
        outbox(new synchronized<queue_type>())
    { }

    template<class U>
    typename std::enable_if<
        std::is_convertible<typename pristine<U>::type, T>::value,
        streamed&
    >::type
    write(U&& value) {
        outbox->synchronize()->template append<typename protocol::chunk>(std::forward<U>(value));
        return *this;
    }

    streamed&
    abort(int code, const std::string& reason) {
        outbox->synchronize()->template append<typename protocol::error>(code, reason);
        return *this;
    }

    streamed&
    close() {
        outbox->synchronize()->template append<typename protocol::choke>();
        return *this;
    }

    template<class UpstreamType>
    void
    attach(UpstreamType&& upstream) const {
        outbox->synchronize()->attach(std::move(upstream));
    }

private:
    const std::shared_ptr<synchronized<queue_type>> outbox;
};

} // namespace cocaine

#endif
