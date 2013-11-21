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

#ifndef COCAINE_IO_STREAMED_SLOT_HPP
#define COCAINE_IO_STREAMED_SLOT_HPP

#include "cocaine/rpc/slots/deferred.hpp"

namespace cocaine {

template<class T>
struct streamed {
    typedef io::message_queue<io::streaming_tag<T>> queue_type;
    typedef io::streaming<T> protocol;

    template<template<class> class, class, class> friend struct io::deferred_slot;

    streamed():
        queue_impl(std::make_shared<synchronized<queue_type>>())
    { }

    template<class U>
    void
    write(U&& value,
          typename std::enable_if<std::is_convertible<typename pristine<U>::type, T>::value>::type* = nullptr)
    {
        (*queue_impl)->template append<typename protocol::chunk>(std::forward<U>(value));
    }

    void
    abort(int code, const std::string& reason) {
        (*queue_impl)->template append<typename protocol::error>(code, reason);
    }

    void
    close() {
        (*queue_impl)->template append<typename protocol::choke>();
    }

private:
    const std::shared_ptr<synchronized<queue_type>> queue_impl;
};

} // namespace cocaine

#endif
