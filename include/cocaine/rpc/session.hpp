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

#ifndef COCAINE_IO_SESSION_HPP
#define COCAINE_IO_SESSION_HPP

#include "cocaine/common.hpp"

#include <mutex>

namespace cocaine {

class session_t {
    std::unique_ptr<io::channel<io::socket<io::tcp>>> ptr;
    std::mutex mutex;

    // Root dispatch

    const std::shared_ptr<io::dispatch_t> prototype;

    // Downstreams

    class downstream_t;

    std::map<uint64_t, std::shared_ptr<downstream_t>> downstreams;

private:
    void
    invoke(const io::message_t& message, const std::shared_ptr<session_t>& self);

    void
    revoke();

public:
    friend class actor_t;
    friend class upstream_t;

    session_t(std::unique_ptr<io::channel<io::socket<io::tcp>>>&& ptr_, const std::shared_ptr<io::dispatch_t>& prototype_):
        ptr(std::move(ptr_)),
        prototype(prototype_)
    { }

    void
    detach(uint64_t tag) {
        downstreams.erase(tag);
    }
};

} // namespace cocaine

#endif