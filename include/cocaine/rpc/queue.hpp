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

#ifndef COCAINE_IO_MESSAGE_QUEUE_HPP
#define COCAINE_IO_MESSAGE_QUEUE_HPP

#include "cocaine/rpc/protocol.hpp"
#include "cocaine/rpc/upstream.hpp"

#include "cocaine/tuple.hpp"

#include <deque>
#include <mutex>

#include <boost/mpl/lambda.hpp>
#include <boost/mpl/transform.hpp>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/variant.hpp>

namespace cocaine { namespace io {

// Message queue

template<class Tag> class message_queue;

namespace aux {

template<class Event>
struct frozen {
    template<typename... Args>
    frozen(bool seals_, Args&&... args):
        seals(seals_),
        tuple(std::forward<Args>(args)...)
    { }

    // NOTE: Indicates that this event should seal the upstream.
    bool seals;

    // NOTE: If the event cannot be sent right away, simply move the event arguments to a temporary
    // storage and wait for the upstream to be attached.
    typename tuple::fold<typename event_traits<Event>::tuple_type>::type tuple;
};

struct frozen_visitor_t:
    public boost::static_visitor<void>
{
    frozen_visitor_t(const std::shared_ptr<upstream_t>& upstream_):
        upstream(upstream_)
    { }

    template<class Event>
    void
    operator()(const frozen<Event>& event) const {
        if(!event.seals) {
            upstream->send<Event>(event.tuple);
        } else {
            upstream->seal<Event>(event.tuple);
        }
    }

private:
    const std::shared_ptr<upstream_t>& upstream;
};

} // namespace aux

template<class Tag>
class message_queue {
    typedef typename boost::mpl::transform<
        typename protocol<Tag>::messages,
        typename boost::mpl::lambda<aux::frozen<boost::mpl::arg<1>>>
    >::type wrapped_type;

    typedef typename boost::make_variant_over<wrapped_type>::type variant_type;

    // Operation log.
    std::deque<variant_type> operations;

    // The upstream might be attached during state method invocation, so it has to be synchronized
    // with a mutex - the atomicicity guarantee of the shared_ptr<T> is not enough.
    std::shared_ptr<upstream_t> upstream;
    std::mutex mutex;

public:
    template<class Event, typename... Args>
    void
    append(bool seals, Args&&... args) {
        if(upstream) {
            if(!seals) {
                upstream->send<Event>(std::forward<Args>(args)...);
            } else {
                upstream->seal<Event>(std::forward<Args>(args)...);
            }
        } else {
            operations.emplace_back(aux::frozen<Event>(seals, std::forward<Args>(args)...));
        }
    }

    void
    attach(const std::shared_ptr<upstream_t>& upstream_) {
        upstream = upstream_;

        if(operations.empty()) {
            return;
        }

        aux::frozen_visitor_t visitor(upstream);

        for(auto it = operations.begin(); it != operations.end(); ++it) {
            boost::apply_visitor(visitor, *it);
        }

        operations.clear();
    }

public:
    void lock() { mutex.lock(); }
    void unlock() { mutex.unlock(); }
};

}} // namespace cocaine::io

#endif
