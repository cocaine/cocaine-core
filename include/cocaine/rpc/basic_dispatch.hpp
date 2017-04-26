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

#ifndef COCAINE_IO_BASIC_DISPATCH_HPP
#define COCAINE_IO_BASIC_DISPATCH_HPP

#include "cocaine/common.hpp"
#include "cocaine/rpc/asio/decoder.hpp"
#include "cocaine/rpc/graph.hpp"

#include <boost/optional/optional_fwd.hpp>

#include <string>

namespace cocaine {

template<class Tag> class dispatch;

namespace io {

class basic_dispatch_t {
    // The name of the service which this protocol implementation belongs to. Mostly used for logs,
    // and for synchronization stuff in the Locator Service.
    const std::string m_name;

public:
    explicit
    basic_dispatch_t(const std::string& name);

    virtual
    ~basic_dispatch_t();

    virtual
    auto
    attached(std::shared_ptr<session_t> session) -> void;

    // Concrete protocol transition as opposed to transition description in protocol graphs. It can
    // either be some new dispatch pointer, an uninitialized pointer - terminal transition, or just
    // an empty optional - recurrent transition (i.e. no transition at all).

    virtual
    boost::optional<dispatch_ptr_t>
    process(const decoder_t::message_type& message, const upstream_ptr_t& upstream) = 0;

    // Called on abnormal transport destruction. The idea's if the client disconnects unexpectedly,
    // i.e. not reaching the end of the dispatch graph, then some special handling might be needed.
    // Think 'zookeeper ephemeral nodes'.

    virtual
    void
    discard(const std::error_code& ec);

    // Observers

    virtual
    auto
    root() const -> const graph_root_t& = 0;

    auto
    name() const -> std::string;

    virtual
    int
    version() const = 0;
};

} // namespace io
} // namespace cocaine

#endif
