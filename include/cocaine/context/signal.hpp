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

    async_visitor(const tuple_type& args_, asio::io_service& asio_, std::weak_ptr<dispatch<typename Event::tag>> dispatch_):
        args(args_),
        asio(asio_),
        slot_dispatch(std::move(dispatch_))
    { }

    template<class Other>
    result_type
    operator()(const std::shared_ptr<io::basic_slot<Other>>& COCAINE_UNUSED_(slot)) const {
        __builtin_unreachable();
    }

    result_type
    operator()(const std::shared_ptr<io::basic_slot<Event>>& slot) const {
        auto args = this->args;

        auto dispatch_copy = slot_dispatch;
        asio.post([=]() mutable {
            if(dispatch_copy.lock()) {
                (*slot)({}, std::move(args), upstream<void>());
            }
        });
    }

    const tuple_type& args;
    asio::io_service& asio;
    const std::weak_ptr<dispatch<typename Event::tag>> slot_dispatch;
};

template<class Tag>
struct event_visitor:
    public boost::static_visitor<void>
{
    event_visitor(const std::shared_ptr<dispatch<Tag>>& slot_, asio::io_service& asio_):
        slot(slot_),
        asio(asio_)
    { }

    template<class Event>
    result_type
    operator()(const io::frozen<Event>& event) const {
        try {
            slot->process(io::event_traits<Event>::id, async_visitor<Event>(event.tuple, asio, slot));
        } catch(const std::system_error& e) {
            if(e.code() != error::slot_not_found) throw;
        }
    }

private:
    const std::shared_ptr<dispatch<Tag>>& slot;
    asio::io_service& asio;
};

template<class Event>
struct history_traits {
    template<class HistoryType, class VariantType>
    static
    void
    apply(HistoryType& history, VariantType&& variant) {
        history.emplace_back(std::move(variant));
    }
};

} // namespace aux

template<class Tag>
class retroactive_signal {
    typedef typename io::make_frozen_over<Tag>::type variant_type;

    struct subscriber_t {
        std::weak_ptr<dispatch<Tag>> slot;
        asio::io_service& asio;
    };

    std::mutex mutex;

    std::list<variant_type> history;
    std::list<subscriber_t> subscribers;

public:
    void
    listen(const std::shared_ptr<dispatch<Tag>>& slot, asio::io_service& asio) {
        std::lock_guard<std::mutex> guard(mutex);

        auto visitor = aux::event_visitor<Tag>(slot, asio);

        for(auto it = history.begin(); it != history.end(); ++it) {
            boost::apply_visitor(visitor, *it);
        }

        subscribers.emplace_back(subscriber_t{slot, asio});
    }

    template<class Event, class... Args>
    void
    invoke(Args&&... args) {
        std::lock_guard<std::mutex> guard(mutex);

        auto variant = variant_type(io::make_frozen<Event>(std::forward<Args>(args)...));

        for(auto it = subscribers.begin(); it != subscribers.end();) {
            auto slot = it->slot.lock();

            if(!slot) {
                it = subscribers.erase(it); continue;
            }

            boost::apply_visitor(aux::event_visitor<Tag>(slot, it->asio), variant);

            ++it;
        }
        aux::history_traits<Event>::apply(history, std::move(variant));
    }
};

} // namespace cocaine

#endif
