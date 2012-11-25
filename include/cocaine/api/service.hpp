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

#ifndef COCAINE_SERVICE_API_HPP
#define COCAINE_SERVICE_API_HPP

#include "cocaine/common.hpp"
#include "cocaine/asio.hpp"
#include "cocaine/context.hpp"
#include "cocaine/io.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/repository.hpp"

#include "cocaine/helpers/json.hpp"

#include <boost/function.hpp>
#include <boost/function_types/function_type.hpp>

#include <boost/mpl/insert_range.hpp>

namespace cocaine { namespace api {

namespace detail {
    struct dispatch_base_t {
        virtual
        ~dispatch_base_t() {
            // Empty.
        }

        virtual
        void
        operator()(const msgpack::object& object) = 0;
    };

    template<class Event>
    struct dispatch:
        public dispatch_base_t
    {
        typedef typename io::event_traits<
            Event
        >::tuple_type tuple_type;

        // Constructing callable type from the event tuple type

        typedef typename boost::function_types::function_type<
            typename boost::mpl::insert_range<
                tuple_type,
                typename boost::mpl::begin<tuple_type>::type,
                typename boost::mpl::list<void>::type
            >::type
        >::type function_type;

        typedef boost::function<
            function_type
        > callable_type;

        dispatch(callable_type callable):
            m_callable(callable)
        { }

        virtual
        void
        operator()(const msgpack::object& packed) {
            if(packed.type != msgpack::type::ARRAY ||
               packed.via.array.size != io::event_traits<Event>::length)
            {
                throw msgpack::type_error();
            }

            typedef typename boost::mpl::begin<tuple_type>::type begin;
            typedef typename boost::mpl::end<tuple_type>::type end;

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
            void
            apply(callable_type& callable,
                  msgpack::object * packed,
                  Args&&... args)
            {
                typedef typename boost::mpl::deref<It>::type element_type;
                typedef typename boost::mpl::next<It>::type next_type;
                
                element_type element;

                io::type_traits<element_type>::unpack(*packed, element);

                return invoke<next_type, End>::apply(
                    callable,
                    ++packed,
                    std::forward<Args>(args)...,
                    std::move(element)
                );
            }
        };

        template<class End>
        struct invoke<End, End> {
            template<typename... Args>
            static inline
            void
            apply(callable_type& callable,
                  msgpack::object*,
                  Args&&... args)
            {
                callable(std::forward<Args>(args)...);
            }
        };

    private:
        callable_type m_callable;
    };
}

template<class Tag>
class service:
    public boost::noncopyable
{
    public:
        virtual
        ~service() {
            // Empty.
        }

        virtual
        void
        run() {
            m_loop.loop();
        }

    protected:
        service(context_t& context,
                const std::string& name,
                const Json::Value& /* args */):
            m_context(context),
            m_log(context.log("service/" + name)),
            m_channel(context, ZMQ_ROUTER),
            m_watcher(m_loop),
            m_checker(m_loop)
        {
            std::string endpoint = cocaine::format(
                "ipc://%1%/service:%2%",
                m_context.config.ipc_path,
                name
            );

            try {
                m_channel.bind(endpoint);
            } catch(const zmq::error_t& e) {
                throw configuration_error_t(
                    "unable to bind the '%s' service channel - %s",
                    name,
                    e.what()
                );
            }

            m_watcher.set<service, &service::on_event>(this);
            m_watcher.start(m_channel.fd(), ev::READ);
            m_checker.set<service, &service::on_check>(this);
            m_checker.start();
        }

        logging::logger_t*
        log() {
            return m_log.get();
        }

        template<class Event>
        void
        bind(typename detail::dispatch<Event>::callable_type callable) {
            m_dispatch.emplace(
                io::event_traits<Event>::id,
                boost::make_shared<detail::dispatch<Event>>(callable)
            );
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
                    
                    if(!m_channel.recv_multipart(source, message_id)) {
                        return;
                    }
                }

                COCAINE_LOG_DEBUG(
                    m_log,
                    "received type %d message from %s",
                    message_id,
                    source
                );

                zmq::message_t message;
                msgpack::unpacked unpacked;

                // Will be properly unpacked in the dispatch object.
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

                dispatch_map_t::iterator it = m_dispatch.find(message_id);

                if(it != m_dispatch.end()) {
                    (*it->second)(unpacked.get());
                } else {
                    COCAINE_LOG_ERROR(
                        m_log,
                        "no dispatch for event type %d, dropping",
                        message_id
                    );
                }
            } while(--counter);
        }

    private:
        context_t& m_context;
        boost::shared_ptr<logging::logger_t> m_log;

        // Engine I/O

        typedef io::channel<
            Tag,
            io::policies::unique
        > rpc_channel_t;

        rpc_channel_t m_channel;
        
        // Event loop

        ev::dynamic_loop m_loop;
        
        ev::io m_watcher;
        ev::prepare m_checker;

        // Handling

        typedef std::map<
            int,
            boost::shared_ptr<detail::dispatch_base_t>
        > dispatch_map_t;

        dispatch_map_t m_dispatch;
};

template<class Tag>
struct category_traits<service<Tag>> {
    typedef std::unique_ptr<service<Tag>> ptr_type;

    struct factory_type:
        public factory_base<service<Tag>>
    {
        virtual
        ptr_type
        get(context_t& context,
            const std::string& name,
            const Json::Value& args) = 0;
    };

    template<class T>
    struct default_factory:
        public factory_type
    {
        virtual
        ptr_type
        get(context_t& context,
            const std::string& name,
            const Json::Value& args)
        {
            return ptr_type(
                new T(context, name, args)
            );
        }
    };
};

}} // namespace cocaine::api

#endif
