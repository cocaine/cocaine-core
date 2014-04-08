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

#include <boost/mpl/front_inserter.hpp>
#include <boost/mpl/copy.hpp>

#include <functional>
#include <type_traits>

namespace cocaine { namespace raft {

// Some types of log entry used by RAFT.
struct entry_tag;

struct node_commands {
    // This log entry does nothing. Leader tries to commit this entry at beginning of new term
    // (see commitment restriction).
    struct nop {
        typedef entry_tag tag;

        static
        const char*
        alias() {
            return "nop";
        }

        typedef void result_type;
    };

    // Add new node to configuration.
    // This command moves configuration to transitional state, if configuration doesn't contain this node.
    struct insert {
        typedef entry_tag tag;

        static
        const char*
        alias() {
            return "insert";
        }

        typedef boost::mpl::list<
            node_id_t
        > tuple_type;

        typedef void result_type;
    };

    // Remove node from configuration.
    // This command moves configuration to transitional state, if configuration contains this node.
    struct erase {
        typedef entry_tag tag;

        static
        const char*
        alias() {
            return "erase";
        }

        typedef boost::mpl::list<
            node_id_t
        > tuple_type;

        typedef void result_type;
    };

    // Commit new configuration.
    struct commit {
        typedef entry_tag tag;

        static
        const char*
        alias() {
            return "commit";
        }

        typedef void result_type;
    };
};

} // namespace raft

namespace io {

template<>
struct protocol<cocaine::raft::entry_tag> {
    typedef boost::mpl::int_<
        1
    >::type version;

    typedef boost::mpl::list<
        cocaine::raft::node_commands::nop,
        cocaine::raft::node_commands::insert,
        cocaine::raft::node_commands::erase,
        cocaine::raft::node_commands::commit
    > messages;

    typedef cocaine::raft::node_commands type;
};

} // namespace io

namespace raft {

namespace detail {

    template<class Value>
    struct result_traits {
        typedef boost::variant<Value, std::error_code> result_type;
    };

    template<>
    struct result_traits<void> {
        typedef std::error_code result_type;
    };

} // namespace detail

// Some types related to command stored in log entry.
template<class Event>
struct command_traits {
    typedef typename Event::result_type value_type;
    typedef typename detail::result_traits<value_type>::result_type result_type;
    typedef std::function<void(const result_type&)> callback_type;
};

// Entry of state machine log.
// It can store command to state machine, configuration change command or NOP (see above).
template<class StateMachine>
class log_entry {
    typedef StateMachine machine_type;

    typedef typename boost::mpl::transform<
        typename io::protocol<typename machine_type::tag>::messages,
        typename boost::mpl::lambda<io::aux::frozen<boost::mpl::arg<1>>>
    >::type machine_events_list;

    typedef typename boost::mpl::transform<
        typename io::protocol<entry_tag>::messages,
        typename boost::mpl::lambda<io::aux::frozen<boost::mpl::arg<1>>>
    >::type entry_events_list;

    // List of all command types, which may be stored in the entry.
    //typedef typename boost::mpl::joint_view<machine_events_list, entry_events_list>::type
    //        commands_list;
    typedef typename boost::mpl::copy<
                entry_events_list,
                boost::mpl::front_inserter<machine_events_list>
            >::type
            commands_list;

    // We need callback type to store information about command type.
    template<class Event>
    struct handler_wrapper {
        typename command_traits<Event>::callback_type callback;
    };

    template<class Command>
    struct handler_type {
        typedef handler_wrapper<typename Command::event_type> type;
    };

    typedef typename boost::make_variant_over<typename boost::mpl::transform<
                commands_list,
                boost::mpl::lambda<handler_type<boost::mpl::arg<1>>>
            >::type>::type
            handlers_type;

    // This visitor applies the entry to state machine and provides result to handler.
    template<class Machine>
    struct value_visitor :
        public boost::static_visitor<>
    {
        value_visitor(handlers_type &handler, const Machine &machine):
            m_handler(handler),
            m_machine(machine)
        { }

        template<class Event>
        typename std::enable_if<
            !std::is_same<typename command_traits<Event>::value_type, void>::value
        >::type
        operator()(const io::aux::frozen<Event>& command) const {
            auto handler = boost::get<handler_wrapper<Event>>(&m_handler);
            if (handler && handler->callback) {
                handler->callback(m_machine(command));
            } else {
                m_machine(command);
            }
        }

        template<class Event>
        typename std::enable_if<
            std::is_same<typename command_traits<Event>::value_type, void>::value
        >::type
        operator()(const io::aux::frozen<Event>& command) const {
            m_machine(command);
            auto handler = boost::get<handler_wrapper<Event>>(&m_handler);
            if (handler && handler->callback) {
                handler->callback(std::error_code());
            }
        }

    private:
        handlers_type &m_handler;
        const Machine &m_machine;
    };

    // This visitor provides error to handler.
    struct set_error_visitor :
        public boost::static_visitor<>
    {
        set_error_visitor(const std::error_code& ec):
            m_ec(ec)
        { }

        template<class Event>
        void
        operator()(const handler_wrapper<Event>& handler) const {
            if (handler.callback) {
                handler.callback(m_ec);
            }
        }

    private:
        const std::error_code& m_ec;
    };

public:
    typedef typename boost::make_variant_over<commands_list>::type command_type;

public:
    log_entry():
        m_term(0),
        m_value(io::aux::make_frozen<node_commands::nop>())
    { }

    log_entry(uint64_t term, const command_type& value):
        m_term(term),
        m_value(value)
    { }

    uint64_t
    term() const {
        return m_term;
    }

    command_type&
    value() {
        return m_value;
    }

    const command_type&
    value() const {
        return m_value;
    }

    template<class Event>
    void
    bind(const typename command_traits<Event>::callback_type& handler) {
        m_handler = handler_wrapper<Event>{handler};
    }

    template<class Visitor>
    void
    visit(const Visitor& visitor) {
        boost::apply_visitor(value_visitor<Visitor>(m_handler, visitor), m_value);
        m_handler = handler_wrapper<node_commands::nop>();
    }

    void
    set_error(const std::error_code& ec) {
        boost::apply_visitor(set_error_visitor(ec), m_handler);
        m_handler = handler_wrapper<node_commands::nop>();
    }

private:
    uint64_t m_term;
    command_type m_value;
    handlers_type m_handler;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_ENTRY_HPP
