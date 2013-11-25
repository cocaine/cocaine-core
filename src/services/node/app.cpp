/*
    Copyright (c) 2011-2013 Andrey Sibiryov <me@kobology.ru>
    Copyright (c) 2011-2013 Other contributors as noted in the AUTHORS file.

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

#include "cocaine/app.hpp"

#include "cocaine/api/isolate.hpp"

#include "cocaine/asio/acceptor.hpp"
#include "cocaine/asio/reactor.hpp"
#include "cocaine/asio/local.hpp"
#include "cocaine/asio/socket.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/actor.hpp"

#include "cocaine/detail/services/node/engine.hpp"
#include "cocaine/detail/services/node/event.hpp"
#include "cocaine/detail/services/node/manifest.hpp"
#include "cocaine/detail/services/node/messages.hpp"
#include "cocaine/detail/services/node/profile.hpp"
#include "cocaine/detail/services/node/stream.hpp"

#include "cocaine/dispatch.hpp"

#include "cocaine/idl/node.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/memory.hpp"

#include "cocaine/rpc/channel.hpp"

#include "cocaine/traits/dynamic.hpp"
#include "cocaine/traits/literal.hpp"

#include <tuple>

#define BOOST_BIND_NO_PLACEHOLDERS
#include <boost/bind.hpp>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::io;

namespace fs = boost::filesystem;

namespace {

class streaming_service_t:
    public implements<io::event_traits<io::app::enqueue>::transition_type>
{
    const api::stream_ptr_t downstream;

private:
    void
    write(const std::string& chunk) {
        downstream->write(chunk.data(), chunk.size());
    }

    void
    error(int code, const std::string& reason) {
        downstream->error(code, reason);
        downstream->close();
    }

    void
    close() {
        downstream->close();
    }

public:
    typedef io::protocol<io::event_traits<io::app::enqueue>::transition_type>::type protocol;

    struct write_slot_t:
        public basic_slot<protocol::chunk>
    {
        write_slot_t(const std::shared_ptr<streaming_service_t>& self_):
            self(self_)
        { }

        virtual
        std::shared_ptr<dispatch_t>
        operator()(const msgpack::object& unpacked, const std::shared_ptr<upstream_t>& /* upstream */) {
            auto service = self.lock();

            io::invoke<event_traits<rpc::chunk>::tuple_type>::apply(
                boost::bind(&streaming_service_t::write, service.get(), boost::arg<1>()),
                unpacked
            );

            return service;
        }

    private:
        const std::weak_ptr<streaming_service_t> self;
    };

    streaming_service_t(context_t& context, const std::string& name, const api::stream_ptr_t& downstream_):
        implements<io::event_traits<io::app::enqueue>::transition_type>(context, name),
        downstream(downstream_)
    {
        using namespace std::placeholders;

        on<protocol::error>(std::bind(&streaming_service_t::error, this, _1, _2));
        on<protocol::choke>(std::bind(&streaming_service_t::close, this));
    }
};

class app_service_t:
    public implements<io::app_tag>
{
    context_t& context;
    app_t&     app;

private:
    struct stream_adapter_t: public api::stream_t {
        stream_adapter_t(const std::shared_ptr<upstream_t>& upstream_):
            upstream(upstream_)
        { }

        typedef io::protocol<io::event_traits<io::app::enqueue>::drain_type>::type protocol;

        virtual
        void
        write(const char* chunk, size_t size) {
            upstream->send<protocol::chunk>(literal_t { chunk, size });
        }

        virtual
        void
        error(int code, const std::string& reason) {
            upstream->send<protocol::error>(code, reason);
        }

        virtual
        void
        close() {
            upstream->send<protocol::choke>();
        }

    private:
        const std::shared_ptr<upstream_t>& upstream;
    };

    struct enqueue_slot_t:
        public basic_slot<io::app::enqueue>
    {
        enqueue_slot_t(app_service_t& self_):
            self(self_)
        { }

        virtual
        std::shared_ptr<dispatch_t>
        operator()(const msgpack::object& unpacked, const std::shared_ptr<upstream_t>& upstream) {
            return io::invoke<event_traits<io::app::enqueue>::tuple_type>::apply(
                boost::bind(&app_service_t::enqueue, &self, upstream, boost::arg<1>(), boost::arg<2>()),
                unpacked
            );
        }

    private:
        app_service_t& self;
    };

    std::shared_ptr<dispatch_t>
    enqueue(const std::shared_ptr<upstream_t>& upstream, const std::string& event, const std::string& tag) {
        api::stream_ptr_t downstream;

        if(tag.empty()) {
            downstream = app.enqueue(api::event_t(event), std::make_shared<stream_adapter_t>(upstream));
        } else {
            downstream = app.enqueue(api::event_t(event), std::make_shared<stream_adapter_t>(upstream), tag);
        }

        auto service = std::make_shared<streaming_service_t>(context, name(), downstream);

        service->on<streaming_service_t::protocol::chunk>(
            std::make_shared<streaming_service_t::write_slot_t>(service)
        );

        return service;
    }

public:
    app_service_t(context_t& context_, const std::string& name_, app_t& app_):
        implements<io::app_tag>(context_, cocaine::format("service/%1%", name_)),
        context(context_),
        app(app_)
    {
        on<io::app::enqueue>(std::make_shared<enqueue_slot_t>(*this));
        on<io::app::info>(std::bind(&app_t::info, std::ref(app)));
    }
};

} // namespace

app_t::app_t(context_t& context, const std::string& name, const std::string& profile):
    m_context(context),
    m_log(new logging::log_t(context, cocaine::format("app/%1%", name))),
    m_manifest(new manifest_t(context, name)),
    m_profile(new profile_t(context, profile))
{
    auto isolate = m_context.get<api::isolate_t>(
        m_profile->isolate.type,
        m_context,
        m_manifest->name,
        m_profile->isolate.args
    );

    if(m_manifest->source() != sources::cache) {
        isolate->spool();
    }

    std::shared_ptr<io::socket<local>> lhs, rhs;

    // Create the engine control sockets.
    std::tie(lhs, rhs) = io::link<local>();

    m_reactor = std::make_unique<reactor_t>();
    m_engine_control = std::make_unique<channel<io::socket<local>>>(*m_reactor, lhs);

    try {
        m_engine.reset(new engine_t(m_context, std::make_shared<reactor_t>(), *m_manifest, *m_profile, rhs));
    } catch(const std::system_error& e) {
        throw cocaine::error_t(
            "unable to initialize the engine - %s - [%d] %s",
            e.what(),
            e.code().value(),
            e.code().message()
        );
    }
}

app_t::~app_t() {
    // Empty.
}

void
app_t::start() {
    COCAINE_LOG_INFO(m_log, "starting the engine");

    // Start the engine thread.
    m_thread.reset(new std::thread(std::bind(&engine_t::run, m_engine)));

    COCAINE_LOG_DEBUG(m_log, "starting the invocation service");

    // Publish the app service.
    m_context.insert(m_manifest->name, std::make_unique<actor_t>(
        m_context,
        std::make_shared<reactor_t>(),
        std::unique_ptr<dispatch_t>(new app_service_t(m_context, m_manifest->name, *this))
    ));

    COCAINE_LOG_INFO(m_log, "the engine has started");
}

namespace {
    namespace detail {
        template<class It, class End, typename... Args>
        struct fold_impl {
            typedef typename fold_impl<
                typename boost::mpl::next<It>::type,
                End,
                Args...,
                typename std::add_lvalue_reference<
                    typename boost::mpl::deref<It>::type
                >::type
            >::type type;
        };

        template<class End, typename... Args>
        struct fold_impl<End, End, Args...> {
            typedef std::tuple<Args...> type;
        };

        template<class TupleType, int N = std::tuple_size<TupleType>::value>
        struct unfold_impl {
            template<class Event, typename... Args>
            static inline
            void
            apply(const message_t& message,
                  TupleType& tuple,
                  Args&&... args)
            {
                unfold_impl<TupleType, N - 1>::template apply<Event>(
                    message,
                    tuple,
                    std::get<N - 1>(tuple),
                    std::forward<Args>(args)...
                );
            }
        };

        template<class TupleType>
        struct unfold_impl<TupleType, 0> {
            template<class Event, typename... Args>
            static inline
            void
            apply(const message_t& message,
                  TupleType& /* tuple */,
                  Args&&... args)
            {
                message.as<Event>(std::forward<Args>(args)...);
            }
        };
    } // namespace detail

    template<class TypeList>
    struct fold {
        typedef typename detail::fold_impl<
            typename boost::mpl::begin<TypeList>::type,
            typename boost::mpl::end<TypeList>::type
        >::type type;
    };

    template<class TupleType>
    struct unfold {
        template<class Event>
        static inline
        void
        apply(const message_t& message,
              TupleType& tuple)
        {
            return detail::unfold_impl<
                TupleType
            >::template apply<Event>(message, tuple);
        }
    };

    template<class Event>
    struct expect {
        template<class>
        struct result {
            typedef void type;
        };

        template<typename... Args>
        expect(reactor_t& reactor, Args&&... args):
            m_reactor(reactor),
            m_tuple(std::forward<Args>(args)...)
        { }

        expect(expect&& other):
            m_reactor(other.m_reactor),
            m_tuple(std::move(other.m_tuple))
        { }

        expect&
        operator=(expect&& other) {
            m_tuple = std::move(other.m_tuple);
            return *this;
        }

        void
        operator()(const message_t& message) {
            if(message.id() == event_traits<Event>::id) {
                unfold<tuple_type>::template apply<Event>(
                    message,
                    m_tuple
                );

                m_reactor.stop();
            }
        }

        void
        operator()(const std::error_code& ec) {
            throw cocaine::error_t("i/o failure — [%d] %s", ec.value(), ec.message());
        }

    private:
        typedef typename fold<
            typename event_traits<Event>::tuple_type
        >::type tuple_type;

        reactor_t& m_reactor;
        tuple_type m_tuple;
    };
} // namespace

void
app_t::stop() {
    COCAINE_LOG_INFO(m_log, "stopping the engine");

    if(!m_manifest->local) {
        // Destroy the app service.
        m_context.remove(m_manifest->name);
    }

    auto callback = expect<control::terminate>(*m_reactor);

    m_engine_control->rd->bind(std::ref(callback), std::ref(callback));
    m_engine_control->wr->write<control::terminate>(0UL);

    // Blocks until the engine is stopped.
    m_reactor->run();

    m_thread->join();
    m_thread.reset();

    m_engine.reset();

    COCAINE_LOG_INFO(m_log, "the engine has stopped");
}

dynamic_t
app_t::info() const {
    dynamic_t info = dynamic_t::object_t();

    if(!m_thread) {
        info.as_object()["error"] = "the engine is not active";
        return info;
    }

    auto callback = expect<control::info>(*m_reactor, info);

    m_engine_control->rd->bind(std::ref(callback), std::ref(callback));
    m_engine_control->wr->write<control::report>(0UL);

    try {
        // Blocks until either the response or timeout happens.
        m_reactor->run_with_timeout(defaults::control_timeout);
    } catch(const cocaine::error_t& e) {
        info.as_object()["error"] = "the engine is unresponsive";
        return info;
    }

    info.as_object()["profile"] = m_profile->name;

    return info;
}

std::shared_ptr<api::stream_t>
app_t::enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream) {
    return m_engine->enqueue(event, upstream);
}

std::shared_ptr<api::stream_t>
app_t::enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream, const std::string& tag) {
    return m_engine->enqueue(event, upstream, tag);
}
