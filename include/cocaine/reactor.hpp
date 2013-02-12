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
#include "cocaine/api/service.hpp"
#include "cocaine/asio/service.hpp"
#include "cocaine/messaging.hpp"
#include "cocaine/slot.hpp"

#include <functional>
#include <thread>

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

        virtual
        ~reactor_t();

        template<class Event, class F>
        void
        on(F callable);

    public:
        io::service_t&
        service() {
            return m_service;
        }

        const io::service_t&
        service() const {
            return m_service;
        }

    private:
        void
        on_connection(const std::shared_ptr<io::pipe_t>& pipe);

        void
        on_message(const std::shared_ptr<io::codec<io::pipe_t>>& io,
                   const io::message_t& message);

        void
        on_terminate(ev::async&, int);

    private:
        context_t& m_context;
        std::unique_ptr<logging::log_t> m_log;

        // Event loop

        io::service_t m_service;
        ev::async m_terminate;

        // Service I/O

        std::unique_ptr<
            io::connector<io::acceptor_t>
        > m_connector;

        std::set<
            std::shared_ptr<io::codec<io::pipe_t>>
        > m_codecs;

        // RPC

#if BOOST_VERSION >= 103600
        typedef boost::unordered_map<
#else
        typedef std::map<
#endif
            unsigned int,
            std::shared_ptr<slot_base_t>
        > slot_map_t;

        slot_map_t m_slots;

        // Execution context

        std::unique_ptr<std::thread> m_thread;
};

namespace detail {
    template<class F, bool Bound = std::is_bind_expression<F>::value>
    struct result_of {
        typedef typename std::result_of<F>::type type;
    };

    template<class F>
    struct result_of<F, true> {
        typedef typename F::result_type type;
    };
}

template<class Event, class F>
void
reactor_t::on(F callable) {
    typedef typename detail::result_of<F>::type result_type;
    typedef typename io::event_traits<Event>::tuple_type sequence_type;
    typedef slot<result_type, sequence_type> slot_type;

    m_slots.emplace(
        io::event_traits<Event>::id,
        std::make_shared<slot_type>(callable)
    );
}

} // namespace cocaine

#endif
