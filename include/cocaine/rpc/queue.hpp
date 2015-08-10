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

#ifndef COCAINE_IO_MESSAGE_QUEUE_HPP
#define COCAINE_IO_MESSAGE_QUEUE_HPP

#include "cocaine/rpc/frozen.hpp"
#include "cocaine/rpc/tags.hpp"
#include "cocaine/rpc/upstream.hpp"

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

namespace cocaine { namespace io {

template<class Tag, class Upstream = basic_upstream_t> class message_queue;

namespace mpl = boost::mpl;

namespace aux {

template<class Upstream>
struct frozen_visitor:
    public boost::static_visitor<void>
{
    typedef Upstream upstream_type;

    explicit
    frozen_visitor(const std::shared_ptr<upstream_type>& upstream_):
        upstream(upstream_)
    { }

    template<class Event>
    void
    operator()(frozen<Event>& frozen) const {
        upstream->template send<Event>(std::move(frozen.tuple));
    }

private:
    const std::shared_ptr<upstream_type>& upstream;
};

} // namespace aux

template<class Tag, class Upstream>
class message_queue {
    typedef Upstream upstream_type;

    // Operation log.
    std::vector<typename make_frozen_over<Tag>::type> m_operations;

    // The upstream might be attached during message invocation, so it has to be synchronized for
    // thread safety - the atomicity guarantee of the shared_ptr<T> is not enough.
    std::shared_ptr<upstream_type> m_upstream;

public:
    template<class Event, class... Args>
    void
    append(Args&&... args) {
        static_assert(
            std::is_same<typename Event::tag, Tag>::value,
            "message protocol is not compatible with this message queue"
        );

        if(!m_upstream) {
            return m_operations.emplace_back(make_frozen<Event>(std::forward<Args>(args)...));
        }

        m_upstream->template send<Event>(std::forward<Args>(args)...);
    }

    template<class OtherTag>
    void
    attach(upstream<OtherTag>&& upstream) {
        static_assert(
            details::is_compatible<Tag, OtherTag>::value,
            "upstream protocol is not compatible with this message queue"
        );

        if(!m_operations.empty()) {
            aux::frozen_visitor<upstream_type> visitor(upstream.ptr);

            // For some weird reasons, boost::apply_visitor() only accepts lvalue-references to the
            // visitor object, so there's no other choice but to actually bind it to a variable.
            std::for_each(m_operations.begin(), m_operations.end(), boost::apply_visitor(visitor));

            m_operations.clear();
        }

        m_upstream = std::move(upstream.ptr);
    }
};

}} // namespace cocaine::io

#endif
