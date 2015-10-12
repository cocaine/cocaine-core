/*
    Copyright (c) 2011-2015 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_CONTEXT_SIGNAL_HPP
#define COCAINE_CONTEXT_SIGNAL_HPP

#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/frozen.hpp"

#include <algorithm>
#include <list>

namespace cocaine {

template<class Tag> class signal;

namespace aux {

template<class Event>
class async_visitor:
    public boost::static_visitor<void>
{
    typedef typename io::basic_slot<Event>::tuple_type tuple_type;

    const tuple_type& args;
    asio::io_service& asio;

public:
    async_visitor(const tuple_type& args, asio::io_service& asio):
        args(args),
        asio(asio)
    { }

    result_type
    operator()(const std::shared_ptr<io::basic_slot<Event>>& slot) const {
        auto& args = this->args;
        auto  weak = std::weak_ptr<io::basic_slot<Event>>(slot);

        asio.post([weak, args]() {
            // Slot will be gone either because the dispatch was destroyed
            // or because either forget<Event>() or halt() has been called.
            if(auto ptr = weak.lock()) (*ptr)(tuple_type(args), {});
        });
    }

    template<class Other>
    result_type
    operator()(const std::shared_ptr<io::basic_slot<Other>>& /**/) const {
        COCAINE_UNREACHABLE();
    }
};

template<class Tag>
class event_visitor:
    public boost::static_visitor<bool>
{
    typedef std::weak_ptr<const dispatch<Tag>> target_type;

    const target_type weak;
    asio::io_service& asio;

public:
    event_visitor(const target_type& weak, asio::io_service& asio):
        weak(weak),
        asio(asio)
    { }

    template<class Event>
    result_type
    operator()(const io::frozen<Event>& event) const {
        const auto target  = weak.lock();
        const auto visitor = async_visitor<Event>(event.tuple, asio);

        if(target) try {
            target->process(io::event_traits<Event>::id, visitor);
        } catch(const std::system_error& e) {
            if(e.code() != error::slot_not_found) throw;
        }

        return target != nullptr;
    }
};

} // namespace aux

template<class Tag>
class signal {
    typedef typename io::make_frozen_over<Tag>::type variant_type;
    typedef aux::event_visitor<Tag> visitor_type;

    std::list<variant_type> past;
    std::list<visitor_type> visitors;

public:
    typedef std::shared_ptr<const dispatch<Tag>> target_type;

    template<class Event, class... Args>
    void
    invoke(Args&&... args);

    void
    listen(const target_type& target, asio::io_service& asio);
};

template<class Tag> template<class Event, class... Args>
void
signal<Tag>::invoke(Args&&... args) {
    past.emplace_back(io::make_frozen<Event>(
        std::forward<Args>(args)...));

    for(auto it = visitors.begin(); it != visitors.end(); /***/) {
        if(boost::apply_visitor(*it, past.back())) {
            it++; continue;
        }

        it = visitors.erase(it);
    }
}

template<class Tag>
void
signal<Tag>::listen(const target_type& target, asio::io_service& asio) {
    visitors.emplace_back(aux::event_visitor<Tag>(target, asio));

    std::for_each(past.begin(), past.end(), [this](const variant_type& e) {
        boost::apply_visitor(visitors.back(), e);
    });
}

} // namespace cocaine

#endif
