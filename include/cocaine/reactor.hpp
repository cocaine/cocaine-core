/*
    Copyright (c) 2011-2012 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2012 Other contributors as noted in the AUTHORS file.

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

#ifndef COCAINE_REACTOR_HPP
#define COCAINE_REACTOR_HPP

#include "cocaine/common.hpp"
#include "cocaine/asio.hpp"
#include "cocaine/channel.hpp"
#include "cocaine/slot.hpp"

#include "cocaine/api/service.hpp"

#include <boost/thread/thread.hpp>

namespace cocaine {

class reactor_t:
    public api::service_t
{
    public:
        typedef api::service_t category_type;

    public:
        virtual
        void
        run();

        virtual
        void
        terminate();

    protected:
        reactor_t(context_t& context,
                  const std::string& name,
                  const Json::Value& args);

        template<class Event, class F>
        void
        on(F callable);

        ev::loop_ref&
        loop() {
            return m_loop;
        }

        const ev::loop_ref&
        loop() const {
            return m_loop;
        }

    private:
        void
        on_event(ev::io&, int);
        
        void
        on_check(ev::prepare&, int);
        
        void
        on_terminate(ev::async&, int);

        void
        process();

    private:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;
        
        // Service I/O.
        io::shared_channel_t m_channel;
        
        // Event loop.
        ev::dynamic_loop m_loop;
        
        // I/O watchers.
        ev::io m_watcher;
        ev::prepare m_checker;
        ev::async m_terminate;

#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            unsigned int,
            boost::shared_ptr<slot_base_t>
        > slot_map_t;

        // Event slots.
        slot_map_t m_slots;

        // Service thread.
        std::unique_ptr<boost::thread> m_thread;
};

template<class Event, class F>
void
reactor_t::on(F callable) {
    typedef decltype(callable()) result_type;;
    typedef typename io::event_traits<Event>::tuple_type sequence_type;
    typedef slot<result_type, sequence_type> slot_type;

    m_slots.emplace(
        io::event_traits<Event>::id,
        boost::make_shared<slot_type>(callable)
    );
}

} // namespace cocaine

#endif
