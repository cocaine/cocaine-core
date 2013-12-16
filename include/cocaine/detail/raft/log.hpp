/*
    Copyright (c) 2013-2013 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_RAFT_LOG_HPP
#define COCAINE_RAFT_LOG_HPP

#include "cocaine/rpc/queue.hpp"

#include <boost/variant.hpp>
#include <boost/optional.hpp>

#include <vector>
#include <functional>

namespace cocaine { namespace raft {

template<class StateMachine>
class log_entry {
    typedef typename boost::mpl::transform<
        typename protocol<typename StateMachine::tag>::messages,
        typename boost::mpl::lambda<aux::frozen<boost::mpl::arg<1>>>
    >::type wrapped_type;

public:
    typedef typename boost::make_variant_over<wrapped_type>::type command_type;

    struct nop_t {};

    typedef typename boost::variant<nop_t, command_type> variant_type;

public:
    log_entry(uint64_t term):
        m_term(term),
        m_data(nop_t())
    {
        // Empty.
    }

    log_entry(uint64_t term, const command_type& command):
        m_term(term),
        m_data(command)
    {
        // Empty.
    }

    uint64_t
    term() const {
        return m_term;
    }

    bool
    is_command() const {
        return boost::get<command_type>(&m_data);
    }

    const command_type&
    get_command() const {
        return boost::get<command_type>(m_data);
    }

    variant_type&
    get_command() {
        return boost::get<command_type>(m_data);
    }

    template<class Handler>
    void
    bind(Handler&& h) {
        m_commit_handler = h;
    }

    void
    notify(boost::optional<uint64_t> index) {
        if(m_commit_handler) {
            m_commit_handler(index);
        }
    }

private:
    uint64_t m_term;
    variant_type m_data;
    std::function<void(boost::optional<uint64_t>)> m_commit_handler;
};

template<class StateMachine>
class log {
public:
    typedef log_entry<StateMachine> value_type;
    typedef std::vector<value_type>::const_iterator const_iterator;

    template<class... Args>
    void
    append(Args&&... args) {
        m_data.emplace_back(std::forward<Args>(args)...);
    }

    value_type&
    at(uint64_t index) {
        return m_data[index];
    }

    const value_type&
    at(uint64_t index) const {
        return m_data[index];
    }

    const_iterator
    iter(uint64_t index) const {
        return m_data.begin() + index;
    }

    const_iterator
    end(uint64_t index) const {
        return m_data.end();
    }

    uint64_t
    last_index() const {
        return m_data.size();
    }

    uint64_t
    last_term() const {
        return m_data.back().term();
    }

    void
    truncate_suffix(uint64_t index) {
        m_data.resize(index);
    }

private:
    std::vector<value_type> m_data;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_LOG_HPP
