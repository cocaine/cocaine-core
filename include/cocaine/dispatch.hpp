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

#include "cocaine/rpc/graph.hpp"
#include "cocaine/rpc/traversal.hpp"

#include <boost/mpl/apply.hpp>
#include <boost/mpl/empty.hpp>

namespace cocaine { namespace io {

namespace aux {

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
        invoke(const message_t& message, const api::stream_ptr_t& upstream) const;

        virtual
        auto
        protocol() const -> const dispatch_graph_t& = 0;

        virtual
        int
        versions() const = 0;

        std::string
        name() const;

    private:
        const std::unique_ptr<cocaine::logging::log_t> m_log;

        typedef std::map<
            int,
            std::shared_ptr<detail::slot_concept_t>
        > slot_map_t;

        slot_map_t m_slots;

        // It's mutable to enable invoke() to be const.
        mutable std::mutex m_mutex;

        // For actor's named threads feature.
        const std::string m_name;
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
        typedef deferred_slot<deferred<R>, Event> type;
    };
};

} // namespace aux

template<class Event, class F>
void
dispatch_t::on(const F& callable, typename std::enable_if<!aux::is_slot<F>::value>::type*) {
    typedef typename detail::result_of<F>::type result_type;

    typedef typename boost::mpl::apply<
        aux::select<result_type>,
        Event
    >::type slot_type;

    on<Event>(std::make_shared<slot_type>(callable));
}

template<class Event>
void
dispatch_t::on(const std::shared_ptr<detail::slot_concept_t>& ptr) {
    const int id = event_traits<Event>::id;

    std::lock_guard<std::mutex> guard(m_mutex);

    if(m_slots.find(id) != m_slots.end()) {
        throw cocaine::error_t("duplicate slot %d: %s", id, ptr->name());
    }

    m_slots[id] = ptr;
}

template<class Event>
void
dispatch_t::forget() {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_slots.erase(event_traits<Event>::id);
}

} // namespace io

template<class Tag>
struct implementation:
    public io::dispatch_t
{
    implementation(context_t& context, const std::string& name):
        dispatch_t(context, name),
        protograph(io::traverse<Tag>().get())
    {
        static_assert(
            !boost::mpl::empty<typename io::protocol<Tag>::type>::value,
            "protocol has no registered events"
        );
    }

    virtual
    auto
    protocol() const -> const dispatch_graph_t& {
        return protograph;
    }

    virtual
    int
    versions() const {
        return io::protocol<Tag>::version::value;
    }

private:
    const dispatch_graph_t protograph;
};

} // namespace cocaine

#endif
