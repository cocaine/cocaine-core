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

#include "cocaine/rpc/slot/blocking.hpp"
#include "cocaine/rpc/slot/deferred.hpp"
#include "cocaine/rpc/slot/streamed.hpp"

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

    const std::string m_name;

public:
    explicit
    basic_dispatch_t(const std::string& name);

    virtual
   ~basic_dispatch_t();

public:
    auto
    name() const -> std::string;

public:
    typedef boost::optional<dispatch_ptr_t> transition_t;

    virtual
    transition_t
    process(const decoder_t::message_type& message, const upstream_ptr_t& upstream) const = 0;

    virtual
    void
    discard(const boost::system::error_code& COCAINE_UNUSED_(ec)) const {
        // Called on abnormal channel destruction.
    }

    virtual
    auto
    graph() const -> const dispatch_graph_t& = 0;

    virtual
    int
    version() const = 0;
};

typedef basic_dispatch_t::transition_t transition_t;

} // namespace io

namespace mpl = boost::mpl;

template<class Tag>
class dispatch:
    public io::basic_dispatch_t
{
    const io::dispatch_graph_t m_graph;

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
    explicit
    dispatch(const std::string& name):
        basic_dispatch_t(name),
        m_graph(io::traverse<Tag>().get())
    { }

    template<class Event, class F>
    dispatch&
    on(const F& callable, typename boost::disable_if<is_slot<F, Event>>::type* = nullptr);

    template<class Event>
    dispatch&
    on(const std::shared_ptr<io::basic_slot<Event>>& ptr);

    template<class Event>
    void
    forget();

public:
    virtual
    io::transition_t
    process(const io::decoder_t::message_type& message, const io::upstream_ptr_t& upstream) const;

    virtual
    auto
    graph() const -> const io::dispatch_graph_t& {
        return m_graph;
    }

    virtual
    int
    version() const {
        return io::protocol<Tag>::version::value;
    }

private:
    template<class Visitor>
    typename Visitor::result_type
    visit(int id, const Visitor& visitor) const;
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
    calling_visitor_t(const msgpack::object& unpacked_, const io::upstream_ptr_t& upstream_):
        unpacked(unpacked_),
        upstream(upstream_)
    { }

    template<class Event>
    result_type
    operator()(const std::shared_ptr<io::basic_slot<Event>>& slot) const {
        typedef io::basic_slot<Event> slot_type;

        // Unpacked arguments storage.
        typename slot_type::tuple_type args;

        try {
            // Unpacks the object into a tuple using the message typelist as opposed to using the
            // plain tuple type traits, in order to support parameter tags, like optional<T>.
            io::type_traits<typename io::event_traits<Event>::tuple_type>::unpack(unpacked, args);
        } catch(const msgpack::type_error& e) {
            throw cocaine::error_t("unable to unpack message arguments");
        }

        // Call the slot with the upstream constrained using the event's drain protocol type tag.
        return result_type((*slot)(std::move(args), typename slot_type::upstream_type(upstream)));
    }

private:
    const msgpack::object& unpacked;
    const io::upstream_ptr_t& upstream;
};

} // namespace aux

template<class Tag>
template<class Event, class F>
dispatch<Tag>&
dispatch<Tag>::on(const F& callable, typename boost::disable_if<is_slot<F, Event>>::type*) {
    typedef typename aux::select<
        typename result_of<F>::type,
        Event
    >::type slot_type;

    return on<Event>(std::make_shared<slot_type>(callable));
}

template<class Tag>
template<class Event>
dispatch<Tag>&
dispatch<Tag>::on(const std::shared_ptr<io::basic_slot<Event>>& ptr) {
    typedef io::event_traits<Event> traits;

    if(!m_slots->insert(std::make_pair(traits::id, ptr)).second) {
        throw cocaine::error_t("duplicate type %d slot: %s", traits::id, Event::alias());
    }

    return *this;
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
dispatch<Tag>::process(const io::decoder_t::message_type& message, const io::upstream_ptr_t& upstream) const {
    return visit(message.type(), aux::calling_visitor_t(message.args(), upstream));
}

template<class Tag>
template<class Visitor>
typename Visitor::result_type
dispatch<Tag>::visit(int id, const Visitor& visitor) const {
    typename slot_map_t::const_iterator lb, ub;

    std::tie(lb, ub) = m_slots->equal_range(id);

    if(lb == ub) {
        throw cocaine::error_t("unbound type %d slot", id);
    }

    // NOTE: The slot pointer is copied here so that the handling code could unregister the slot via
    // dispatch<T>::forget() without pulling the object from underneath itself.
    typename slot_map_t::mapped_type slot = lb->second;

    try {
        return boost::apply_visitor(visitor, slot);
    } catch(const std::exception& e) {
        // This happens only when the underlying slot has miserably failed to manage its exceptions.
        // In such case, the client is disconnected to prevent any further damage.
        throw cocaine::error_t("unable to invoke type %d slot - %s", id, e.what());
    } catch(...) {
        throw cocaine::error_t("unable to invoke type %d slot", id);
    }
}

} // namespace cocaine

#endif
