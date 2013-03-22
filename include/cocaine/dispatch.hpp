/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/rpc/protocol.hpp"
#include "cocaine/slot.hpp"

namespace cocaine {

class dispatch_t:
    boost::noncopyable
{
    public:
        dispatch_t(context_t& context,
                   const std::string& name);

        virtual
       ~dispatch_t();

        template<class Event, class F>
        void
        on(const std::string& name, F callable);

        template<class Event>
        void
        forget();

        void
        dispatch(const io::message_t& message,
                 const api::stream_ptr_t& upstream) const;

    private:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;

#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            int,
            std::shared_ptr<slot_concept_t>
        > slot_map_t;

        slot_map_t m_slots;
};

namespace detail {
    template<class R>
    struct select {
        template<class Sequence>
        struct apply {
            typedef blocking_slot<R, Sequence> type;
        };
    };

    template<class R>
    struct select<deferred<R>> {
        template<class Sequence>
        struct apply {
            typedef deferred_slot<deferred<R>, Sequence> type;
        };
    };
}

template<class Event, class F>
void
dispatch_t::on(const std::string& name, F callable) {
    typedef typename detail::result_of<F>::type result_type;
    typedef typename io::event_traits<Event>::tuple_type sequence_type;

    typedef typename detail::select<result_type>::template apply<
        sequence_type
    >::type slot_type;

    m_slots.emplace(
        io::event_traits<Event>::id,
        std::make_shared<slot_type>(name, callable)
    );
}

template<class Event>
void
dispatch_t::forget() {
    m_slots.erase(io::event_traits<Event>::id);
}

} // namespace cocaine

#endif
