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

#include <boost/mpl/push_front.hpp>

#include <functional>
#include <type_traits>

namespace cocaine { namespace raft {

// Some types of log entry used by RAFT.

// This log entry does nothing. Leader tries to commit this entry at beginning of new term
// (see commitment restriction).
struct nop_t {
    bool
    operator==(const nop_t&) const {
        return true;
    }
};

// Add new node to configuration.
// This command moves configuration to transitional state, if configuration doesn't contain this node.
struct insert_node_t {
    bool
    operator==(const insert_node_t& other) const {
        return node == other.node;
    }

    node_id_t node;
};

// Remove node from configuration.
// This command moves configuration to transitional state, if configuration contains this node.
struct erase_node_t {
    bool
    operator==(const erase_node_t& other) const {
        return node == other.node;
    }

    node_id_t node;
};

// Commit new configuration.
struct commit_node_t {
    bool
    operator==(const commit_node_t&) const {
        return true;
    }
};

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
template<class Command>
struct command_traits {
    typedef void value_type;
    typedef typename detail::result_traits<value_type>::result_type result_type;
    typedef std::function<void(const result_type&)> callback_type;
};

template<class Event>
struct command_traits<io::aux::frozen<Event>> {
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
    >::type events_list;

    // List of all command types, which may be stored in the entry.
    typedef typename boost::mpl::push_front<
            typename boost::mpl::push_front<
            typename boost::mpl::push_front<
            typename boost::mpl::push_front<
                events_list,
                commit_node_t>::type,
                erase_node_t>::type,
                insert_node_t>::type,
                nop_t>::type
            commands_list;

    // We need callback type to store information about command type.
    template<class Command>
    struct handler_wrapper {
        typename command_traits<Command>::callback_type callback;
    };

    template<class Command>
    struct handler_type {
        typedef handler_wrapper<Command> type;
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

        template<class Command>
        typename std::enable_if<
            !std::is_same<typename command_traits<Command>::value_type, void>::value
        >::type
        operator()(const Command& command) const {
            auto handler = boost::get<handler_wrapper<Command>>(&m_handler);
            if (handler && handler->callback) {
                handler->callback(m_machine(command));
            } else {
                m_machine(command);
            }
        }

        template<class Command>
        typename std::enable_if<
            std::is_same<typename command_traits<Command>::value_type, void>::value
        >::type
        operator()(const Command& command) const {
            m_machine(command);
            auto handler = boost::get<handler_wrapper<Command>>(&m_handler);
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

        template<class Command>
        void
        operator()(const handler_wrapper<Command>& handler) const {
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
        m_value(nop_t())
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

    template<class Command>
    void
    bind(const typename command_traits<Command>::callback_type& handler) {
        m_handler = handler_wrapper<Command>{handler};
    }

    template<class Visitor>
    void
    visit(const Visitor& visitor) {
        boost::apply_visitor(value_visitor<Visitor>(m_handler, visitor), m_value);
        m_handler = handler_wrapper<nop_t>();
    }

    void
    set_error(const std::error_code& ec) {
        boost::apply_visitor(set_error_visitor(ec), m_handler);
        m_handler = handler_wrapper<nop_t>();
    }

private:
    uint64_t m_term;
    command_type m_value;
    handlers_type m_handler;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_ENTRY_HPP
