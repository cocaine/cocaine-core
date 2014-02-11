/*
    Copyright (c) 2013-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_RAFT_ENTRY_HPP
#define COCAINE_RAFT_ENTRY_HPP

#include "cocaine/rpc/queue.hpp"

#include <functional>

namespace cocaine { namespace raft {

// Special type of log entry, which does nothing.
struct nop_t {
    bool
    operator==(const nop_t&) const {
        return true;
    }
};

// Some types related to an operation of state machine.
template<class Event>
struct command_traits {
    typedef typename Event::result_type result_type;
    typedef boost::variant<result_type, std::error_code> variant_type;
    typedef std::function<void(const variant_type&)> callback_type;
};

template<class StateMachine>
class log_entry {
    typedef StateMachine machine_type;

    typedef typename boost::mpl::transform<
        typename io::protocol<typename machine_type::tag>::messages,
        typename boost::mpl::lambda<io::aux::frozen<boost::mpl::arg<1>>>
    >::type wrapped_type;

    template<class Event>
    struct handler_wrapper {
        typename command_traits<Event>::callback_type callback;
    };

    template<class Event>
    struct handler_type {
        typedef handler_wrapper<Event> type;
    };

    typedef typename boost::make_variant_over<typename boost::mpl::transform<
        typename io::protocol<typename machine_type::tag>::messages,
        typename boost::mpl::lambda<handler_type<boost::mpl::arg<1>>>
    >::type>::type handlers_type;

    struct notify_visitor :
        public boost::static_visitor<>
    {
        notify_visitor(const std::error_code& ec):
            m_ec(ec)
        { }

        template<class Event>
        void
        operator()(const handler_wrapper<Event>& h) const {
            h.callback(m_ec);
        }

    private:
        const std::error_code& m_ec;
    };

public:
    typedef typename boost::make_variant_over<wrapped_type>::type command_type;
    typedef typename boost::variant<nop_t, command_type> value_type;

public:
    log_entry():
        m_term(0),
        m_value(nop_t())
    { }

    log_entry(uint64_t term, const value_type& value):
        m_term(term),
        m_value(value)
    { }

    uint64_t
    term() const {
        return m_term;
    }

    value_type&
    value() {
        return m_value;
    }

    const value_type&
    value() const {
        return m_value;
    }

    bool
    is_command() const {
        return boost::get<command_type>(&m_value);
    }

    const command_type&
    get_command() const {
        return boost::get<command_type>(m_value);
    }

    command_type&
    get_command() {
        return boost::get<command_type>(m_value);
    }

    template<class Event>
    void
    bind(const typename command_traits<Event>::callback_type& h) {
        handler_wrapper<Event> wrapper;
        wrapper.callback = h;
        m_handler = wrapper;
    }

    template<class Event, class Result>
    void
    notify(Result&& value) {
        auto handler = boost::get<handler_wrapper<Event>>(&m_handler);
        if(handler && handler->callback) {
            handler->callback(typename command_traits<Event>::variant_type(std::forward<Result>(value)));
            m_handler = handler_wrapper<Event>();
        }
    }

    void
    notify(const std::error_code& ec) {
        boost::apply_visitor(notify_visitor(ec), m_handler);
    }

private:
    uint64_t m_term;
    value_type m_value;
    handlers_type m_handler;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_ENTRY_HPP
