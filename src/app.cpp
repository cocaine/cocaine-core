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

#include "cocaine/api/driver.hpp"
#include "cocaine/api/event.hpp"

#include "cocaine/asio/acceptor.hpp"
#include "cocaine/asio/local.hpp"
#include "cocaine/asio/socket.hpp"
#include "cocaine/asio/tcp.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/actor.hpp"
#include "cocaine/detail/archive.hpp"
#include "cocaine/detail/engine.hpp"
#include "cocaine/detail/manifest.hpp"
#include "cocaine/detail/profile.hpp"
#include "cocaine/detail/traits/json.hpp"

#include "cocaine/dispatch.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/rpc/channel.hpp"

#include <tuple>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::io;

namespace fs = boost::filesystem;

struct app_t::invocation_service_t:
    public dispatch_t
{
    struct invocation_t:
        public slot_concept_t
    {
        invocation_t(invocation_service_t& self):
            slot_concept_t("invoke"),
            m_self(self)
        { }

        virtual
        void
        operator()(const msgpack::object& unpacked, const api::stream_ptr_t& upstream) {
            typedef event_traits<app::invoke>::tuple_type tuple_type;

            std::string event;
            std::string blob;

            // NOTE: No try-block here, as the enclosing dispatch is handling all exceptions.
            type_traits<tuple_type>::unpack(unpacked, event, blob);

            try {
                m_self.enqueue(api::event_t(event), upstream)->write(blob.data(), blob.size());
            } catch(const cocaine::error_t& e) {
                upstream->error(resource_error, e.what());
                upstream->close();
            }
        }

    private:
        invocation_service_t& m_self;
    };

    invocation_service_t(context_t& context, const std::string& name, app_t& app):
        dispatch_t(context, cocaine::format("service/%1%", name)),
        m_app(app)
    {
        on<app::invoke>(std::make_shared<invocation_t>(*this));
    }

private:
    std::shared_ptr<api::stream_t>
    enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream) {
        return m_app.enqueue(event, upstream);
    }

private:
    app_t& m_app;
};

app_t::app_t(context_t& context, const std::string& name, const std::string& profile):
    m_context(context),
    m_log(new logging::log_t(context, cocaine::format("app/%1%", name))),
    m_manifest(new manifest_t(context, name)),
    m_profile(new profile_t(context, profile))
{
    fs::path path = fs::path(m_context.config.path.spool) / name;

    if(!fs::exists(path) || m_manifest->source() != sources::cache) {
        try {
            fs::remove_all(path);
        } catch(const fs::filesystem_error& e) {
            throw cocaine::error_t("unable to clean up the app spool %s - %s", path, e.what());
        }

        deploy(name, path.string());
    }

    if(!fs::exists(m_manifest->slave)) {
        throw configuration_error_t("executable '%s' does not exist", m_manifest->slave);
    }

    m_reactor.reset(new reactor_t());
}

app_t::~app_t() {
    stop();
}

void
app_t::start() {
    if(m_thread) {
        return;
    }

    COCAINE_LOG_INFO(m_log, "starting the engine");

    auto reactor = std::make_shared<reactor_t>();
    auto drivers = driver_map_t();

    if(!m_manifest->drivers.empty()) {
        COCAINE_LOG_INFO(
            m_log,
            "starting %llu %s",
            m_manifest->drivers.size(),
            m_manifest->drivers.size() == 1 ? "driver" : "drivers"
        );

        api::category_traits<api::driver_t>::ptr_type driver;

        for(auto it = m_manifest->drivers.begin(); it != m_manifest->drivers.end(); ++it) {
            const std::string name = cocaine::format(
                "%s/%s",
                m_manifest->name,
                it->first
            );

            try {
                driver = m_context.get<api::driver_t>(
                    it->second.type,
                    m_context,
                    *reactor,
                    *this,
                    name,
                    it->second.args
                );
            } catch(const cocaine::error_t& e) {
                throw configuration_error_t("unable to initialize the '%s' driver - %s", name, e.what());
            } catch(...) {
                throw configuration_error_t("unable to initialize the '%s' driver - unknown exception");
            }

            drivers[it->first] = std::move(driver);
        }
    }

    std::shared_ptr<io::socket<local>> lhs, rhs;

    // Create the engine control sockets.
    std::tie(lhs, rhs) = io::link<local>();

    m_engine_control.reset(new channel<io::socket<local>>(*m_reactor, lhs));
    m_engine.reset(new engine_t(m_context, reactor, *m_manifest, *m_profile, rhs));

    // We can safely swap the current driver set now.
    m_drivers.swap(drivers);

    // Start the engine thread.
    m_thread.reset(new std::thread(std::bind(&engine_t::run, m_engine)));

    if(!m_manifest->local) {
        std::vector<io::tcp::endpoint> endpoints = {
            { "0.0.0.0", 0 }
        };

        // Initialize the app invocation service.
        auto service = std::unique_ptr<actor_t>(new actor_t(
            m_context,
            std::make_shared<reactor_t>(),
            std::unique_ptr<invocation_service_t>(
                new invocation_service_t(m_context, m_manifest->name, *this)
            ),
            endpoints
        ));

        service->run();

        // Publish it in the cluster.
        m_context.attach(m_manifest->name, std::move(service));
    }

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
    }

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
}

void
app_t::stop() {
    if(!m_thread) {
        return;
    }

    COCAINE_LOG_INFO(m_log, "stopping the engine");

    if(!m_manifest->local) {
        // Stop the app invocation service.
        m_context.detach(m_manifest->name)->terminate();
    }

    auto callback = expect<control::terminate>(*m_reactor);

    m_engine_control->rd->bind(std::ref(callback), std::ref(callback));
    m_engine_control->wr->write<control::terminate>(0UL);

    try {
        // Blocks until either the response or timeout happens.
        m_reactor->run(/* defaults::control_timeout */);
    } catch(const cocaine::error_t& e) {
        throw cocaine::error_t("the engine is unresponsive - %s", e.what());
    }

    m_thread->join();
    m_thread.reset();

    COCAINE_LOG_INFO(m_log, "the engine has stopped");

    // NOTE: Stop the drivers, so that there won't be any open
    // sockets and so on while the engine is stopped.
    m_drivers.clear();

    // NOTE: Destroy the engine last, because it holds the only
    // reference to the reactor which drivers use.
    m_engine.reset();
}

Json::Value
app_t::info() const {
    Json::Value info(Json::objectValue);

    if(!m_thread) {
        info["error"] = "the engine is not active";
        return info;
    }

    auto callback = expect<control::info>(*m_reactor, info);

    m_engine_control->rd->bind(std::ref(callback), std::ref(callback));
    m_engine_control->wr->write<control::report>(0UL);

    try {
        // Blocks until either the response or timeout happens.
        m_reactor->run(/* defaults::control_timeout */);
    } catch(const cocaine::error_t& e) {
        info["error"] = "the engine is unresponsive";
        return info;
    }

    info["profile"] = m_profile->name;

    for(auto it = m_drivers.begin(); it != m_drivers.end(); ++it) {
        info["drivers"][it->first] = it->second->info();
    }

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

void
app_t::deploy(const std::string& name, const std::string& path) {
    std::string blob;

    COCAINE_LOG_INFO(m_log, "deploying the app to '%s'", path);

    auto storage = api::storage(m_context, "core");

    try {
        blob = storage->get<std::string>("apps", name);
    } catch(const storage_error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to fetch the app from the storage - %s", e.what());
        throw configuration_error_t("the '%s' app is not available", name);
    }

    try {
        archive_t archive(m_context, blob);
        archive.deploy(path);
    } catch(const archive_error_t& e) {
        COCAINE_LOG_ERROR(m_log, "unable to extract the app files - %s", e.what());
        throw configuration_error_t("the '%s' app is not available", name);
    }
}
