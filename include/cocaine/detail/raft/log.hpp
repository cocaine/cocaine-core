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

#include "cocaine/common.hpp"
#include "cocaine/rpc/queue.hpp"

namespace cocaine { namespace raft {

template<class Tag>
class log_entry {
    typedef typename boost::mpl::transform<
        typename protocol<Tag>::messages,
        typename boost::mpl::lambda<aux::frozen<boost::mpl::arg<1>>>
    >::type wrapped_type;

public:
    typedef typename boost::make_variant_over<wrapped_type>::type data_type;

    struct nop_t {};

    typedef typename boost::variant<nop_t, data_type> variant_type;

public:
    log_entry(uint64_t term, const data_type& data):
        m_term(term),
        m_data(data)
    {
        // Empty.
    }

    uint64_t
    term() const {
        return m_term;
    }

    const variant_type&
    data() const {
        return m_data;
    }

    variant_type&
    data() {
        return m_data;
    }

private:
    uint64_t m_term;
    variant_type m_data;
};

template<class Tag>
class log {
public:
    typedef log_entry<Tag> value_type;

    template<class... Args>
    void
    append(Args&&... args) {
        m_data.emplace_back(std::forward<Args>(args));
    }

    value_type&
    at(uint64_t index) {
        return m_data[index];
    }

    const value_type&
    at(uint64_t index) const {
        return m_data.at(index);
    }

    uint64_t
    last_index() const {
        return m_data.size();
    }

    uint64_t
    last_term() const {
        return m_data.back().term();
    }

private:
    std::vector<value_type> m_data;
};

}} // namespace cocaine::raft

#endif // COCAINE_RAFT_LOG_HPP
