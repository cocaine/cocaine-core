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

#ifndef COCAINE_REACTOR_API_HPP
#define COCAINE_REACTOR_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/asio.hpp"
#include "cocaine/context.hpp"
#include "cocaine/io.hpp"
#include "cocaine/logging.hpp"

#include <boost/function.hpp>
#include <boost/function_types/function_type.hpp>

#include <boost/mpl/push_front.hpp>
#include <boost/mpl/size.hpp>

namespace cocaine { namespace api {

namespace detail {
    namespace ft = boost::function_types;
    namespace mpl = boost::mpl;

    template<class Sequence, typename R = void>
    struct callable {
        typedef typename ft::function_type<
            typename mpl::push_front<Sequence, R>::type
        >::type function_type;

        typedef boost::function<function_type> type;
    };

    template<typename R = void>
    struct slot_base {
        virtual
        ~slot_base() {
            // Empty.
        }

        virtual
        R
        operator()(const msgpack::object& object) = 0;
    };

    template<class Event, typename R = void>
    struct slot:
        public slot_base<R>
    {
        typedef typename io::event_traits<Event>::tuple_type tuple_type;
        typedef typename callable<tuple_type, R>::type callable_type;

        slot(callable_type callable):
            m_callable(callable)
        { }

        virtual
        R
        operator()(const msgpack::object& packed) {
            if(packed.type != msgpack::type::ARRAY ||
               packed.via.array.size != io::event_traits<Event>::length)
            {
                throw msgpack::type_error();
            }

            typedef typename mpl::begin<tuple_type>::type begin;
            typedef typename mpl::end<tuple_type>::type end;

            return invoke<begin, end>::apply(
                m_callable,
                packed.via.array.ptr
            );
        }

    private:
        template<class It, class End>
        struct invoke {
            template<typename... Args>
            static inline
            R
            apply(callable_type& callable,
                  msgpack::object * packed,
                  Args&&... args)
            {
                typedef typename mpl::deref<It>::type argument_type;
                typedef typename mpl::next<It>::type next_type;
                
                argument_type argument;

                io::type_traits<argument_type>::unpack(*packed++, argument);

                return invoke<next_type, End>::apply(
                    callable,
                    packed,
                    std::forward<Args>(args)...,
                    std::move(argument)
                );
            }
        };

        template<class End>
        struct invoke<End, End> {
            template<typename... Args>
            static inline
            R
            apply(callable_type& callable,
                  msgpack::object * packed,
                  Args&&... args)
            {
                return callable(std::forward<Args>(args)...);
            }
        };

    private:
        callable_type m_callable;
    };
}

template<class Tag>
class reactor:
    public boost::noncopyable
{
    protected:
        reactor(context_t& context,
                const std::string& name,
                const Json::Value& args):
            m_context(context),
            m_log(context.log("service/" + name)),
            m_channel(context, ZMQ_ROUTER),
            m_watcher(m_loop),
            m_checker(m_loop)
        {
            std::string endpoint = cocaine::format(
                "ipc://%1%/services/%2%",
                m_context.config.path.runtime,
                name
            );

            try {
                m_channel.bind(endpoint);
            } catch(const zmq::error_t& e) {
                throw configuration_error_t(
                    "unable to bind the '%s' service channel at '%s' - %s",
                    name,
                    endpoint,
                    e.what()
                );
            }

            m_watcher.set<reactor, &reactor::on_event>(this);
            m_watcher.start(m_channel.fd(), ev::READ);
            m_checker.set<reactor, &reactor::on_check>(this);
            m_checker.start();

            const size_t length = boost::mpl::size<
                typename io::dispatch<Tag>::category
            >::value;

            m_slots.reserve(length);
        }

        template<class Event>
        void
        on(typename detail::slot<Event>::callable_type callable) {
            m_slots[io::event_traits<Event>::id] = boost::make_shared<
                detail::slot<Event>
            >(callable);
        }
        
        void
        loop() {
            m_loop.loop();
        }

    public:
        context_t&
        context() {
            return m_context;
        }

        boost::shared_ptr<logging::logger_t>
        log() {
            return m_log;
        }

    private:
        void
        on_event(ev::io&, int) {
            m_checker.stop();

            if(m_channel.pending()) {
                m_checker.start();
                process_events();    
            }
        }
        
        void
        on_check(ev::prepare&, int) {
            m_loop.feed_fd_event(m_channel.fd(), ev::READ);
        }
        
        void
        process_events() {
            int counter = defaults::io_bulk_size;
            
            do {
                // TEST: Ensure that we haven't missed something in a previous iteration.
                BOOST_ASSERT(!m_channel.more());
            
                std::string source;
                int message_id = -1;
                
                {
                    io::scoped_option<
                        io::options::receive_timeout
                    > option(m_channel, 0);
                    
                    if(!m_channel.recv_multipart(io::protect(source), message_id)) {
                        return;
                    }
                }

                zmq::message_t message;
                msgpack::unpacked unpacked;

                // Will be properly unpacked in the slot object.
                m_channel.recv(message);

                try {
                    msgpack::unpack(
                        &unpacked,
                        static_cast<const char*>(message.data()),
                        message.size()
                    );
                } catch(const msgpack::type_error& e) {
                    throw cocaine::error_t("corrupted object");
                } catch(const std::bad_cast& e) {
                    throw cocaine::error_t("corrupted object - type mismatch");
                }

                slot_map_t::const_reference slot = m_slots[message_id];

                if(slot) {
                    (*slot)(unpacked.get());
                } else {
                    COCAINE_LOG_WARNING(
                        m_log,
                        "no slot bound to process message type %d",
                        message_id
                    );
                }
            } while(--counter);
        }

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        // Service I/O

        typedef io::channel<
            Tag,
            io::policies::unique
        > rpc_channel_t;

        rpc_channel_t m_channel;
        
        // Event loop

        ev::dynamic_loop m_loop;
        
        ev::io m_watcher;
        ev::prepare m_checker;

        // Event slots

        typedef std::vector<
            boost::shared_ptr<detail::slot_base<void>>
        > slot_map_t;

        slot_map_t m_slots;
};

}} // namespace cocaine::api

#endif
