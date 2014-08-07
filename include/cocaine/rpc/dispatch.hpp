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

#ifndef COCAINE_IO_DISPATCH_HPP
#define COCAINE_IO_DISPATCH_HPP

#include "cocaine/common.hpp"
#include "cocaine/locked_ptr.hpp"

#include "cocaine/rpc/graph.hpp"
#include "cocaine/rpc/message.hpp"

#include "cocaine/rpc/slots/blocking.hpp"
#include "cocaine/rpc/slots/deferred.hpp"
#include "cocaine/rpc/slots/streamed.hpp"

#include "cocaine/rpc/traversal.hpp"

#include "cocaine/traits/tuple.hpp"

#include <boost/mpl/transform.hpp>
#include <boost/mpl/lambda.hpp>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/variant.hpp>

namespace cocaine {

template<class Tag> class dispatch;

namespace io {

class basic_dispatch_t {
    COCAINE_DECLARE_NONCOPYABLE(basic_dispatch_t)

    // For actor's named threads feature.
    const std::string m_name;

public:
    basic_dispatch_t(const std::string& name);

    virtual
   ~basic_dispatch_t();

public:
    auto
    name() const -> std::string;

public:
    typedef boost::optional<std::shared_ptr<const basic_dispatch_t>> transition_t;

    virtual
    transition_t
    call(const message_t& message, const std::shared_ptr<basic_upstream_t>& upstream) const = 0;

    virtual
    auto
    protocol() const -> const dispatch_graph_t& = 0;

    virtual
    int
    versions() const = 0;
};

typedef basic_dispatch_t::transition_t transition_t;

} // namespace io

namespace mpl = boost::mpl;

template<class Tag>
class dispatch:
    public io::basic_dispatch_t
{
    const io::dispatch_graph_t graph;

    // Slot construction

    template<class Event>
    struct make_slot_over {
        typedef std::shared_ptr<io::basic_slot<Event>> type;
    };

    typedef typename mpl::transform<
        typename io::protocol<Tag>::messages,
        typename mpl::lambda<make_slot_over<mpl::_1>>::type
    >::type slot_types;

    typedef std::map<
        int,
        typename boost::make_variant_over<slot_types>::type
    > slot_map_t;

    synchronized<slot_map_t> m_slots;

    // Slot traits

    template<class T, class Event>
    struct is_slot:
        public std::false_type
    { };

    template<class T, class Event>
    struct is_slot<std::shared_ptr<T>, Event>:
        public std::is_base_of<io::basic_slot<Event>, T>
    { };

public:
    dispatch(const std::string& name):
        basic_dispatch_t(name),
        graph(io::traverse<Tag>().get())
    { }

    template<class Event, class F>
    void
    on(const F& callable, typename std::enable_if<!is_slot<F, Event>::value>::type* = nullptr);

    template<class Event>
    void
    on(const std::shared_ptr<io::basic_slot<Event>>& ptr);

    template<class Visitor>
    typename Visitor::result_type
    invoke(int id, const Visitor& visitor) const;

    template<class Event>
    void
    forget();

public:
    virtual
    io::transition_t
    call(const io::message_t& message, const std::shared_ptr<io::basic_upstream_t>& upstream) const;

    virtual
    auto
    protocol() const -> const io::dispatch_graph_t& {
        return graph;
    }

    virtual
    int
    versions() const {
        return io::protocol<Tag>::version::value;
    }
};

namespace aux {

// Slot selection

template<class R, class Event>
struct select {
    typedef io::blocking_slot<Event> type;
};

template<class R, class Event>
struct select<deferred<R>, Event> {
    typedef io::deferred_slot<deferred, Event> type;
};

template<class R, class Event>
struct select<streamed<R>, Event> {
    typedef io::deferred_slot<streamed, Event> type;
};

// Slot invocation with arguments provided as a MessagePack object

struct calling_visitor_t:
    public boost::static_visitor<io::transition_t>
{
    calling_visitor_t(const msgpack::object& unpacked_, const std::shared_ptr<io::basic_upstream_t>& upstream_):
        unpacked(unpacked_),
        upstream(upstream_)
    { }

    template<class Event>
    result_type
    operator()(const std::shared_ptr<io::basic_slot<Event>>& slot) const {
        typedef io::basic_slot<Event> slot_type;

        // Unpacked arguments storage.
        typename slot_type::tuple_type args;

        // Unpacks the object into a tuple using the message typelist as opposed to using the plain
        // tuple type traits, in order to support parameter tags, like optional<T>.
        io::type_traits<typename io::event_traits<Event>::tuple_type>::unpack(unpacked, args);

        // Call the slot with the upstream constrained using the event's drain protocol type tag.
        return result_type((*slot)(std::move(args), typename slot_type::upstream_type(upstream)));
    }

private:
    const msgpack::object& unpacked;
    const std::shared_ptr<io::basic_upstream_t>& upstream;
};

} // namespace aux

template<class Tag>
template<class Event, class F>
void
dispatch<Tag>::on(const F& callable, typename std::enable_if<!is_slot<F, Event>::value>::type*) {
    typedef typename aux::select<
        typename result_of<F>::type,
        Event
    >::type slot_type;

    on<Event>(std::make_shared<slot_type>(callable));
}

template<class Tag>
template<class Event>
void
dispatch<Tag>::on(const std::shared_ptr<io::basic_slot<Event>>& ptr) {
    if(!m_slots->insert(std::make_pair(io::event_traits<Event>::id, ptr)).second) {
        throw cocaine::error_t("duplicate type %d slot: %s", io::event_traits<Event>::id, ptr->name());
    }
}

template<class Tag>
template<class Visitor>
typename Visitor::result_type
dispatch<Tag>::invoke(int id, const Visitor& visitor) const {
    typename slot_map_t::const_iterator lb, ub;

    std::tie(lb, ub) = m_slots->equal_range(id);

    if(lb == ub) {
        // TODO: COCAINE-82 adds a 'client' error category.
        throw cocaine::error_t("unbound type %d slot", id);
    }

    // NOTE: The slot pointer is copied here so that the handling code could unregister the slot via
    // dispatch<T>::forget() without pulling the object from underneath itself.
    typename slot_map_t::mapped_type slot = lb->second;

    try {
        return boost::apply_visitor(visitor, slot);
    } catch(const std::exception& e) {
        // TODO: COCAINE-82 adds a 'server' error category.
        // This happens only when the underlying slot has miserably failed to manage its exceptions.
        // In such case, the client is disconnected to prevent any further damage.
        throw cocaine::error_t("unable to invoke type %d slot - %s", id, e.what());
    }
}

template<class Tag>
template<class Event>
void
dispatch<Tag>::forget() {
    if(!m_slots->erase(io::event_traits<Event>::id)) {
        throw cocaine::error_t("type %d slot does not exist", io::event_traits<Event>::id);
    }
}

template<class Tag>
io::transition_t
dispatch<Tag>::call(const io::message_t& message, const std::shared_ptr<io::basic_upstream_t>& upstream) const {
    return invoke(message.id(), aux::calling_visitor_t(message.args(), upstream));
}

} // namespace cocaine

#endif
