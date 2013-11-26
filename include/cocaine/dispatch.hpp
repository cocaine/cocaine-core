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

#include "cocaine/rpc/slots/blocking.hpp"
#include "cocaine/rpc/slots/deferred.hpp"
#include "cocaine/rpc/slots/streamed.hpp"

#include "cocaine/rpc/traversal.hpp"

#include <boost/mpl/apply.hpp>
#include <boost/mpl/empty.hpp>

namespace cocaine {

class upstream_t;

namespace io { namespace aux {

template<class T>
struct is_slot:
    public std::false_type
{ };

template<class T>
struct is_slot<std::shared_ptr<T>>:
    public std::is_base_of<detail::slot_concept_t, T>
{ };

} // namespace aux

class dispatch_t {
    COCAINE_DECLARE_NONCOPYABLE(dispatch_t)

    const std::unique_ptr<logging::log_t> m_log;

    typedef std::map<
        int,
        std::shared_ptr<detail::slot_concept_t>
    > slot_map_t;

    synchronized<slot_map_t> m_slots;

    // For actor's named threads feature.
    const std::string m_name;

public:
    dispatch_t(context_t& context, const std::string& name);

    virtual
   ~dispatch_t();

    template<class Event, class F>
    void
    on(const F& callable, typename std::enable_if<!aux::is_slot<F>::value>::type* = nullptr);

    template<class Event>
    void
    on(const std::shared_ptr<detail::slot_concept_t>& ptr);

    template<class Event>
    void
    forget();

public:
    std::shared_ptr<dispatch_t>
    invoke(const message_t& message, const std::shared_ptr<upstream_t>& upstream) const;

    virtual
    auto
    protocol() const -> const dispatch_graph_t& = 0;

    virtual
    int
    versions() const = 0;

    std::string
    name() const;
};

namespace aux {

template<class R>
struct select {
    template<class Event>
    struct apply {
        typedef blocking_slot<R, Event> type;
    };
};

template<class R>
struct select<deferred<R>> {
    template<class Event>
    struct apply {
        typedef deferred_slot<deferred, deferred<R>, Event> type;
    };
};

template<class R>
struct select<streamed<R>> {
    template<class Event>
    struct apply {
        typedef deferred_slot<streamed, streamed<R>, Event> type;
    };
};

} // namespace aux

template<class Event, class F>
void
dispatch_t::on(const F& callable, typename std::enable_if<!aux::is_slot<F>::value>::type*) {
    typedef typename boost::mpl::apply<
        aux::select<typename result_of<F>::type>,
        Event
    >::type slot_type;

    on<Event>(std::make_shared<slot_type>(callable));
}

template<class Event>
void
dispatch_t::on(const std::shared_ptr<detail::slot_concept_t>& ptr) {
    const int id = event_traits<Event>::id;

    if(!m_slots->insert(std::make_pair(id, ptr)).second) {
        throw cocaine::error_t("duplicate slot %d: %s", id, ptr->name());
    }
}

template<class Event>
void
dispatch_t::forget() {
    const int id = event_traits<Event>::id;

    if(!m_slots->erase(id)) {
        throw cocaine::error_t("slot %d does not exist", id);
    }
}

} // namespace io

template<class Tag>
struct implements:
    public io::dispatch_t
{
    implements(context_t& context, const std::string& name):
        dispatch_t(context, name),
        graph(io::traverse<Tag>().get())
    {
        static_assert(
            !boost::mpl::empty<typename io::protocol<Tag>::messages>::value,
            "protocol has no registered events"
        );
    }

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

private:
    const io::dispatch_graph_t graph;
};

} // namespace cocaine

#endif
