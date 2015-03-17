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

#ifndef COCAINE_SIGNAL_HPP
#define COCAINE_SIGNAL_HPP

#include "cocaine/tuple.hpp"

#include <algorithm>
#include <list>
#include <mutex>

#define BOOST_BIND_NO_PLACEHOLDERS
#include <boost/signals2/signal.hpp>

namespace cocaine {

namespace signals = boost::signals2;

template<
    class Signature,
    class IndexSequence = typename make_index_sequence<signals::signal<Signature>::arity>::type
>
class retroactive_signal;

template<typename... Args, size_t... Indices>
class retroactive_signal<void(Args...), index_sequence<Indices...>> {
    typedef signals::signal<void(Args...)> signal_type;

    // The actual underlying signal object.
    signal_type wrapped;

    // Deferred signal arguments.
    mutable std::list<std::tuple<Args...>> history;
    mutable std::mutex mutex;

public:
    typedef typename signal_type::slot_type slot_type;

    auto
    connect(const slot_type& slot) -> signals::connection {
        try {
            std::lock_guard<std::mutex> guard(mutex);

            std::for_each(history.begin(), history.end(), [&slot](const std::tuple<Args...>& args) {
                slot(std::get<Indices>(args)...);
            });
        } catch(const signals::expired_slot& e) {
            return signals::connection();
        }

        return wrapped.connect(slot);
    }

    void
    operator()(Args&&... args) {
        {
            std::lock_guard<std::mutex> guard(mutex);
            history.emplace_back(std::forward<Args>(args)...);
        }

        wrapped(std::forward<Args>(args)...);
    }

    void
    operator()(Args&&... args) const {
        {
            std::lock_guard<std::mutex> guard(mutex);
            history.emplace_back(std::forward<Args>(args)...);
        }

        wrapped(std::forward<Args>(args)...);
    }
};

} // namespace cocaine

#endif
