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
#include "cocaine/traits/frozen.hpp"
#include "cocaine/traits/variant.hpp"

#include <boost/optional.hpp>

#include <vector>
#include <functional>

namespace cocaine {

namespace raft {

struct nop_t {
    bool
    operator==(const nop_t&) const {
        return true;
    }
};

template<class StateMachine>
class log_entry {
    template<class> friend struct cocaine::io::type_traits;

    typedef typename boost::mpl::transform<
        typename io::protocol<typename StateMachine::tag>::messages,
        typename boost::mpl::lambda<io::aux::frozen<boost::mpl::arg<1>>>
    >::type wrapped_type;

public:
    typedef typename boost::make_variant_over<wrapped_type>::type command_type;
    typedef typename boost::variant<nop_t, command_type> value_type;

public:
    explicit
    log_entry(uint64_t term = 0):
        m_term(term),
        m_value(nop_t())
    {
        // Empty.
    }

    log_entry(uint64_t term, const command_type& entry):
        m_term(term),
        m_value(entry)
    {
        // Empty.
    }

    uint64_t
    term() const {
        return m_term;
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
    value_type m_value;
    std::function<void(boost::optional<uint64_t>)> m_commit_handler;
};

template<class StateMachine>
class log {
public:
    typedef log_entry<StateMachine> value_type;
    typedef typename std::vector<value_type>::iterator iterator;
    typedef typename std::vector<value_type>::const_iterator const_iterator;

    bool
    empty() const {
        return m_data.empty();
    }

    value_type&
    at(uint64_t index) {
        return m_data[index];
    }

    const value_type&
    at(uint64_t index) const {
        return m_data[index];
    }

    const value_type&
    back() const {
        return m_data.back();
    }

    value_type&
    back() {
        return m_data.back();
    }

    const_iterator
    iter(uint64_t index) const {
        return m_data.begin() + index;
    }

    const_iterator
    end() const {
        return m_data.end();
    }

    iterator
    iter(uint64_t index) {
        return m_data.begin() + index;
    }

    iterator
    end() {
        return m_data.end();
    }

    uint64_t
    last_index() const {
        return m_data.size() - 1;
    }

    uint64_t
    last_term() const {
        return m_data.back().term();
    }

    template<class... Args>
    void
    append(Args&&... args) {
        m_data.emplace_back(std::forward<Args>(args)...);
    }

    void
    truncate_suffix(uint64_t index) {
        m_data.resize(index);
    }

private:
    std::vector<value_type> m_data;
};

}// namespace raft

namespace io {

template<class StateMachine>
struct type_traits<cocaine::raft::log_entry<StateMachine>> {
    typedef typename cocaine::raft::log_entry<StateMachine>::value_type
            value_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const cocaine::raft::log_entry<StateMachine>& source) {
        target.pack_array(2);
        target << source.term();
        type_traits<value_type>::pack(target, source.m_value);
    }

    static inline
    void
    unpack(const msgpack::object& source, cocaine::raft::log_entry<StateMachine>& target) {
        if(source.type != msgpack::type::ARRAY ||
           source.via.array.size != 2 ||
           source.via.array.ptr[0].type != msgpack::type::POSITIVE_INTEGER)
        {
            throw std::bad_cast();
        }

        target = cocaine::raft::log_entry<StateMachine>(source.via.array.ptr[0].via.u64);

        type_traits<value_type>::unpack(source.via.array.ptr[1], target.m_value);
    }
};

template<>
struct type_traits<cocaine::raft::nop_t> {
    typedef cocaine::raft::nop_t
            value_type;

    template<class Stream>
    static inline
    void
    pack(msgpack::packer<Stream>& target, const value_type& source) {
        target.pack_nil();
    }

    static inline
    void
    unpack(const msgpack::object& source, value_type& target) {
        if(source.type != msgpack::type::NIL) {
            throw std::bad_cast();
        }
    }
};

} // namespace io

} // namespace cocaine

#endif // COCAINE_RAFT_LOG_HPP
