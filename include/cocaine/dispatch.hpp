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

#ifndef COCAINE_DISPATCH_HPP
#define COCAINE_DISPATCH_HPP

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

template<class Tag> class implements;

// Forwards

class upstream_t;

namespace io {

class dispatch_t {
    COCAINE_DECLARE_NONCOPYABLE(dispatch_t)

    const std::unique_ptr<logging::log_t> m_log;

    // For actor's named threads feature.
    const std::string m_name;

public:
    dispatch_t(context_t& context, const std::string& name);

    virtual
   ~dispatch_t();

    // Network I/O-triggered invocation support

    virtual
    std::shared_ptr<dispatch_t>
    invoke(const message_t& message, const std::shared_ptr<upstream_t>& upstream) const = 0;

public:
    virtual
    auto
    protocol() const -> const dispatch_graph_t& = 0;

    virtual
    int
    versions() const = 0;

    std::string
    name() const;
};

} // namespace io

namespace mpl = boost::mpl;

template<class Tag>
class implements:
    public io::dispatch_t
{
    const io::dispatch_graph_t graph;

    typedef typename mpl::transform<
        typename io::protocol<Tag>::messages,
        typename mpl::lambda<std::shared_ptr<io::basic_slot<mpl::_1>>>::type
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
    implements(context_t& context, const std::string& name):
        dispatch_t(context, name),
        graph(io::traverse<Tag>().get())
    { }

    virtual
    std::shared_ptr<io::dispatch_t>
    invoke(const io::message_t& message, const std::shared_ptr<upstream_t>& upstream) const;

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

public:
    template<class Visitor>
    auto
    invoke(int id, Visitor&& visitor) const -> typename Visitor::result_type;

    template<class Event, class F>
    void
    on(const F& callable, typename std::enable_if<!is_slot<F, Event>::value>::type* = nullptr);

    template<class Event>
    void
    on(const std::shared_ptr<io::basic_slot<Event>>& ptr);

    template<class Event>
    void
    forget();
};

namespace aux {

struct invocation_visitor_t:
    public boost::static_visitor<std::shared_ptr<io::dispatch_t>>
{
    invocation_visitor_t(const msgpack::object& unpacked_, const std::shared_ptr<upstream_t>& upstream_):
        unpacked(unpacked_),
        upstream(upstream_)
    { }

    template<class Event>
    std::shared_ptr<io::dispatch_t>
    operator()(const std::shared_ptr<io::basic_slot<Event>>& slot) const {
        typename io::basic_slot<Event>::tuple_type args;

        // Unpacks the object into a tuple using the message typelist as opposed to using the plain
        // tuple type traits, in order to support parameter tags, like optional<T>.
        io::type_traits<typename io::event_traits<Event>::tuple_type>::unpack(unpacked, args);

        return (*slot)(args, upstream);
    }

private:
    const msgpack::object& unpacked;
    const std::shared_ptr<upstream_t>& upstream;
};

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

} // namespace aux

template<class Tag>
std::shared_ptr<io::dispatch_t>
implements<Tag>::invoke(const io::message_t& message, const std::shared_ptr<upstream_t>& upstream) const {
    return invoke(message.id(), aux::invocation_visitor_t(message.args(), upstream));
}

template<class Tag>
template<class Visitor>
auto
implements<Tag>::invoke(int id, Visitor&& visitor) const -> typename Visitor::result_type {
    typename slot_map_t::const_iterator lb, ub;

    std::tie(lb, ub) = m_slots->equal_range(id);

    if(lb == ub) {
        // TODO: COCAINE-82 adds a 'client' error category.
        throw cocaine::error_t("unbound type %d slot", id);
    }

    // NOTE: The slot pointer is copied here so that the handling code could unregister the slot via
    // dispatch_t::forget() without pulling the object from underneath itself.
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
template<class Event, class F>
void
implements<Tag>::on(const F& callable, typename std::enable_if<!is_slot<F, Event>::value>::type*) {
    typedef typename aux::select<
        typename result_of<F>::type,
        Event
    >::type slot_type;

    on<Event>(std::make_shared<slot_type>(callable));
}

template<class Tag>
template<class Event>
void
implements<Tag>::on(const std::shared_ptr<io::basic_slot<Event>>& ptr) {
    if(!m_slots->insert(std::make_pair(io::event_traits<Event>::id, ptr)).second) {
        throw cocaine::error_t("duplicate slot %d: %s", io::event_traits<Event>::id, ptr->name());
    }
}

template<class Tag>
template<class Event>
void
implements<Tag>::forget() {
    if(!m_slots->erase(io::event_traits<Event>::id)) {
        throw cocaine::error_t("slot %d does not exist", io::event_traits<Event>::id);
    }
}

} // namespace cocaine

#endif
