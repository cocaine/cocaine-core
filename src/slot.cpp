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

#include "cocaine/rpc/slots/deferred.hpp"

using namespace cocaine;
using namespace cocaine::io::detail;

struct shared_state_t::result_visitor_t:
    public boost::static_visitor<void>
{
    result_visitor_t(const api::stream_ptr_t& upstream_):
        upstream(upstream_)
    { }

    void
    operator()(const value_type& value) const {
        if(value.size) {
            upstream->write(value.blob, value.size);

            // This memory was allocated by msgpack::sbuffer and has to be freed.
            free(value.blob);
        }

        upstream->close();
    }

    void
    operator()(const error_type& error) const {
        upstream->error(error.code, error.reason);
        upstream->close();
    }

    void
    operator()(const empty_type&) const {
        upstream->close();
    }

private:
    const api::stream_ptr_t& upstream;
};

void
shared_state_t::abort(int code, const std::string& reason) {
    std::lock_guard<std::mutex> guard(mutex);

    if(!result.empty()) return;

    result = error_type { code, reason };
    flush();
}

void
shared_state_t::close() {
    std::lock_guard<std::mutex> guard(mutex);

    if(!result.empty()) return;

    result = empty_type();
    flush();
}

void
shared_state_t::attach(const api::stream_ptr_t& upstream_) {
    std::lock_guard<std::mutex> guard(mutex);

    upstream = upstream_;

    if(!result.empty()) {
        flush();
    }
}

void
shared_state_t::flush() {
    if(upstream) {
        boost::apply_visitor(result_visitor_t(upstream), result);
    }
}
