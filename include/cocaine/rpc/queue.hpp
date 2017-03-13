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

#ifndef COCAINE_IO_MESSAGE_QUEUE_HPP
#define COCAINE_IO_MESSAGE_QUEUE_HPP

#include "cocaine/rpc/frozen.hpp"
#include "cocaine/rpc/tags.hpp"
#include "cocaine/rpc/upstream.hpp"

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

namespace cocaine { namespace io {

template<class Tag> class message_queue;

namespace mpl = boost::mpl;

namespace aux {

struct frozen_visitor:
    public boost::static_visitor<void>
{
    explicit
    frozen_visitor(const std::shared_ptr<basic_upstream_t>& upstream_,
                   hpack::header_storage_t& headers_) :
        upstream(upstream_),
        headers(headers_)
    { }

    template<class Event>
    void
    operator()(frozen<Event>& frozen) const {
        upstream->template send<Event>(std::move(headers), std::move(frozen).tuple);
    }


private:
    const std::shared_ptr<basic_upstream_t>& upstream;
    hpack::header_storage_t& headers;
};

} // namespace aux

template<class Tag>
class message_queue {
    // Operation log.
    std::vector<std::tuple<hpack::header_storage_t, typename make_frozen_over<Tag>::type>> m_operations;

    // The upstream might be attached during message invocation, so it has to be synchronized for
    // thread safety - the atomicity guarantee of the shared_ptr<T> is not enough.
    std::shared_ptr<basic_upstream_t> m_upstream;

public:
    template<class Event, class... Args>
    std::error_code
    append(hpack::header_storage_t headers, Args&&... args) {
        static_assert(std::is_same<typename Event::tag, Tag>::value,
                      "message protocol is not compatible with this message queue");

        if(!m_upstream) {
            m_operations.emplace_back(std::move(headers), make_frozen<Event>(std::forward<Args>(args)...));
            return {};
        }

        try {
            m_upstream->template send<Event>(std::move(headers), std::forward<Args>(args)...);
            return {};
        } catch (const std::system_error& e) {
            return e.code();
        }
    }

    template<class Event, class... Args>
    std::error_code
    append(Args&&... args) {
        static_assert(std::is_same<typename Event::tag, Tag>::value,
                      "message protocol is not compatible with this message queue");

        if(!m_upstream) {
            m_operations.emplace_back(hpack::header_storage_t(), make_frozen<Event>(std::forward<Args>(args)...));
            return {};
        }

        try {
            m_upstream->template send<Event>(std::forward<Args>(args)...);
            return {};
        } catch (const std::system_error& e) {
            return e.code();
        }
    }

    /// This one can throw to propagate exception to session,
    /// as we mainly attach the queue in invocation slot.
    template<class OtherTag>
    void
    attach(upstream<OtherTag>&& upstream) {
        static_assert(details::is_compatible<Tag, OtherTag>::value,
                      "upstream protocol is not compatible with this message queue");

        if(!m_operations.empty()) {

            // For some weird reasons, boost::apply_visitor() only accepts lvalue-references to the
            // visitor object, so there's no other choice but to actually bind it to a variable.
            for (auto& operation : m_operations) {
                aux::frozen_visitor visitor(upstream.ptr, std::get<0>(operation));
                boost::apply_visitor(visitor, std::get<1>(operation));
            }

            m_operations.clear();
        }

        m_upstream = std::move(upstream.ptr);
    }
};

}} // namespace cocaine::io

#endif
