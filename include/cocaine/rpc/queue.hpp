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

#include "cocaine/locked_ptr.hpp"

#include "cocaine/rpc/protocol.hpp"
#include "cocaine/rpc/slot.hpp"
#include "cocaine/rpc/upstream.hpp"

#include "cocaine/tuple.hpp"

#include <deque>

#include <boost/mpl/lambda.hpp>
#include <boost/mpl/transform.hpp>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/variant.hpp>

namespace cocaine { namespace io {

template<class Tag> class message_queue;

namespace mpl = boost::mpl;

namespace aux {

template<class Event>
struct frozen {
    typedef Event event_type;
    typedef typename basic_slot<event_type>::tuple_type tuple_type;

    template<typename... Args>
    frozen(event_type, Args&&... args):
        tuple(std::forward<Args>(args)...)
    { }

    frozen() = default;

    // NOTE: If the message cannot be sent right away, then the message arguments are placed into a
    // temporary storage until the upstream is attached.
    tuple_type tuple;
};

template<class Event, typename... Args>
frozen<Event>
make_frozen(Args&&... args) {
    return frozen<Event>(Event(), std::forward<Args>(args)...);
}

struct frozen_visitor_t:
    public boost::static_visitor<void>
{
    frozen_visitor_t(const std::shared_ptr<basic_upstream_t>& u):
        upstream_(u)
    { }

    template<class Event>
    void
    operator()(const frozen<Event>& frozen) const {
        upstream_->send<Event>(frozen.tuple);
    }

private:
    const std::shared_ptr<basic_upstream_t>& upstream_;
};

} // namespace aux

template<class Tag>
class message_queue {
    typedef typename mpl::transform<
        typename protocol<Tag>::messages,
        typename mpl::lambda<aux::frozen<mpl::_1>>
    >::type frozen_types;

    typedef typename boost::make_variant_over<frozen_types>::type variant_type;

    // Operation log.
    std::deque<variant_type> operations;

    // The upstream might be attached during state method invocation, so it has to be synchronized
    // for thread safety - the atomicicity guarantee of the shared_ptr<T> is not enough.
    std::shared_ptr<basic_upstream_t> upstream_;

public:
    template<class Event, typename... Args>
    void
    append(Args&&... args) {
        static_assert(
            std::is_same<typename Event::tag, Tag>::value,
            "message protocol is not compatible with this message queue"
        );

        if(!upstream_) {
            return operations.emplace_back(aux::make_frozen<Event>(std::forward<Args>(args)...));
        }

        upstream_->send<Event>(std::forward<Args>(args)...);
    }

    template<class OtherTag>
    void
    attach(upstream<OtherTag>&& u) {
        static_assert(
            detail::is_compatible<Tag, OtherTag>::value,
            "upstream protocol is not compatible with this message queue"
        );

        upstream_ = std::move(u.ptr);

        if(operations.empty()) {
            return;
        }

        aux::frozen_visitor_t visitor(upstream_);

        for(auto it = operations.begin(); it != operations.end(); ++it) {
            boost::apply_visitor(visitor, *it);
        }

        operations.clear();
    }
};

}} // namespace cocaine::io

#endif
