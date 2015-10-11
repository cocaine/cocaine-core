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
#include <mutex>

namespace cocaine {

template<class Tag> class retroactive_signal;

namespace aux {

template<class Event>
struct async_visitor:
    public boost::static_visitor<void>
{
    typedef typename io::basic_slot<Event>::tuple_type tuple_type;

    async_visitor(const tuple_type& args_, asio::io_service& asio_):
        args(args_),
        asio(asio_)
    { }

    template<class Other>
    result_type
    operator()(const std::shared_ptr<io::basic_slot<Other>> /**/) const {
        __builtin_unreachable();
    }

    result_type
    operator()(const std::shared_ptr<io::basic_slot<Event>> slot) const {
        auto args = this->args;

        asio.post([=]() mutable {
            (*slot)(std::move(args), upstream<void>());
        });
    }

    const tuple_type& args;
    asio::io_service& asio;
};

template<class Tag>
struct event_visitor:
    public boost::static_visitor<void>
{
    typedef std::shared_ptr<const dispatch<Tag>> focal_type;

    event_visitor(const focal_type& goal_, asio::io_service& asio_):
        goal(goal_),
        asio(asio_)
    { }

    template<class Event>
    result_type
    operator()(const io::frozen<Event>& event) const {
        auto visitor = async_visitor<Event>(event.tuple, asio);

        try {
            goal->process(io::event_traits<Event>::id, visitor);
        } catch(const std::system_error& e) {
            if(e.code() != error::slot_not_found) throw;
        }
    }

private:
    const focal_type& goal;
    asio::io_service& asio;
};

} // namespace aux

template<class Tag>
class retroactive_signal {
    struct subscriber_t {
        std::weak_ptr<const dispatch<Tag>> goal;
        asio::io_service& asio;
    };

    typedef typename io::make_frozen_over<Tag>::type variant_type;

    std::mutex mutex;

    std::list<variant_type> past_events;
    std::list<subscriber_t> subscribers;

public:
    void
    listen(const std::shared_ptr<const dispatch<Tag>>& goal, asio::io_service& asio) {
        std::lock_guard<std::mutex> guard(mutex);

        auto visitor = aux::event_visitor<Tag>(goal, asio);

        for(auto it = past_events.begin(); it != past_events.end(); ++it) {
            boost::apply_visitor(visitor, *it);
        }

        subscribers.emplace_back(subscriber_t{goal, asio});
    }

    template<class Event, class... Args>
    void
    invoke(Args&&... args) {
        std::lock_guard<std::mutex> guard(mutex);

        auto variant = variant_type(io::make_frozen<Event>(std::forward<Args>(args)...));

        for(auto it = subscribers.begin(); it != subscribers.end();) {
            auto goal = it->goal.lock();

            if(!goal) {
                it = subscribers.erase(it); continue;
            }

            boost::apply_visitor(aux::event_visitor<Tag>(goal, it->asio), variant);

            ++it;
        }

        past_events.emplace_back(std::move(variant));
    }
};

} // namespace cocaine

#endif
