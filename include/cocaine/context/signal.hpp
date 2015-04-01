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

#include "cocaine/locked_ptr.hpp"

#include "cocaine/rpc/dispatch.hpp"
#include "cocaine/rpc/frozen.hpp"

#include <algorithm>
#include <list>

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
    operator()(const std::shared_ptr<io::basic_slot<Other>>& COCAINE_UNUSED_(slot)) const {
        __builtin_unreachable();
    }

    result_type
    operator()(const std::shared_ptr<io::basic_slot<Event>>& slot) const {
        auto args = this->args;

        asio.post([slot, args]() mutable {
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
    event_visitor(const std::shared_ptr<dispatch<Tag>>& slot_, asio::io_service& asio_):
        slot(slot_),
        asio(asio_)
    { }

    template<class Event>
    result_type
    operator()(const io::frozen<Event>& event) const {
        try {
            slot->process(io::event_traits<Event>::id, async_visitor<Event>(event.tuple, asio));
        } catch(const std::system_error& e) {
            if(e.code() != error::slot_not_found) throw;
        }
    }

private:
    const std::shared_ptr<dispatch<Tag>>& slot;
    asio::io_service& asio;
};

} // namespace aux

template<class Tag>
class retroactive_signal {
    typedef typename io::make_frozen_over<Tag>::type variant_type;

    struct subscriber_t {
        std::weak_ptr<dispatch<Tag>> slot;
        asio::io_service& asio;
    };

    // Separately synchronized to keep the boost::signals2 guarantees.
    synchronized<std::list<variant_type>> history;
    synchronized<std::list<subscriber_t>> subscribers;

public:
    void
    listen(const std::shared_ptr<dispatch<Tag>>& slot, asio::io_service& asio) {
        subscribers->push_back(subscriber_t{slot, asio});

        auto ptr = history.synchronize();

        std::for_each(ptr->begin(), ptr->end(), [&](const variant_type& event) {
            boost::apply_visitor(aux::event_visitor<Tag>(slot, asio), event);
        });
    }

    template<class Event, class... Args>
    void
    invoke(Args&&... args) {
        auto event = variant_type(io::make_frozen<Event>(std::forward<Args>(args)...));
        auto ptr   = subscribers.synchronize();

        for(auto it = ptr->begin(); it != ptr->end();) {
            auto slot = it->slot.lock();

            if(!slot) {
                it = ptr->erase(it); continue;
            }

            boost::apply_visitor(aux::event_visitor<Tag>(slot, it->asio), event);

            ++it;
        }

        history->emplace_back(std::move(event));
    }
};

} // namespace cocaine

#endif
