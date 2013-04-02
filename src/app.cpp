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

#include "cocaine/asio/acceptor.hpp"
#include "cocaine/asio/local.hpp"
#include "cocaine/asio/socket.hpp"

#include "cocaine/context.hpp"

#include "cocaine/detail/archive.hpp"
#include "cocaine/detail/engine.hpp"
#include "cocaine/detail/manifest.hpp"
#include "cocaine/detail/profile.hpp"

#include "cocaine/detail/traits/json.hpp"

#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/rpc/channel.hpp"

#include <tuple>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

using namespace cocaine;
using namespace cocaine::engine;
using namespace cocaine::io;
using namespace cocaine::logging;

namespace fs = boost::filesystem;

app_t::app_t(context_t& context, const std::string& name, const std::string& profile):
    m_context(context),
    m_log(new log_t(context, cocaine::format("app/%1%", name))),
    m_manifest(new manifest_t(context, name)),
    m_profile(new profile_t(context, profile))
{
    fs::path path = fs::path(m_context.config.path.spool) / name;

    if(!fs::exists(path)) {
        deploy(name, path.string());
    }

    if(!fs::exists(m_manifest->slave)) {
        throw configuration_error_t("executable '%s' does not exist", m_manifest->slave);
    }

    m_reactor.reset(new reactor_t());
}

app_t::~app_t() {
    // NOTE: Stop the engine first, so that there won't be any
    // new events during the drivers shutdown process.
    stop();
}

void
app_t::start() {
    if(m_thread) {
        return;
    }

    COCAINE_LOG_INFO(m_log, "starting the engine");

    auto reactor = std::unique_ptr<reactor_t>(new reactor_t());

    if(!m_manifest->drivers.empty()) {
        COCAINE_LOG_INFO(
            m_log,
            "starting %llu %s",
            m_manifest->drivers.size(),
            m_manifest->drivers.size() == 1 ? "driver" : "drivers"
        );

        api::category_traits<api::driver_t>::ptr_type driver;

        for(config_t::component_map_t::const_iterator it = m_manifest->drivers.begin();
            it != m_manifest->drivers.end();
            ++it)
        {
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
                COCAINE_LOG_ERROR(
                    m_log,
                    "unable to initialize the '%s' driver - %s",
                    name,
                    e.what()
                );

                // NOTE: In order for driver map to be repopulated if the app is restarted.
                m_drivers.clear();

                throw configuration_error_t("unable to initialize the drivers");
            }

            m_drivers[it->first] = std::move(driver);
        }
    }

    std::shared_ptr<io::socket<local>> lhs, rhs;

    // Create the engine control sockets.
    std::tie(lhs, rhs) = io::link<local>();

    m_channel.reset(new channel<io::socket<local>>(
        *m_reactor,
        lhs
    ));

    // NOTE: The event loop is not started here yet.
    m_engine.reset(
        new engine_t(
            m_context,
            std::move(reactor),
            *m_manifest,
            *m_profile,
            rhs
        )
    );

    auto runnable = std::bind(
        &engine_t::run,
        m_engine
    );

    m_thread.reset(new std::thread(runnable));

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
                typename boost::add_reference<
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

    auto callback = expect<control::terminate>(*m_reactor);

    m_channel->rd->bind(std::ref(callback), std::ref(callback));
    m_channel->wr->write<control::terminate>(0UL);

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
}

Json::Value
app_t::info() const {
    Json::Value info(Json::objectValue);

    if(!m_thread) {
        info["error"] = "the engine is not active";
        return info;
    }

    auto callback = expect<control::info>(*m_reactor, info);

    m_channel->rd->bind(std::ref(callback), std::ref(callback));
    m_channel->wr->write<control::report>(0UL);

    try {
        // Blocks until either the response or timeout happens.
        m_reactor->run(/* defaults::control_timeout */);
    } catch(const cocaine::error_t& e) {
        info["error"] = "the engine is unresponsive";
        return info;
    }

    info["profile"] = m_profile->name;

    for(driver_map_t::const_iterator it = m_drivers.begin();
        it != m_drivers.end();
        ++it)
    {
        info["drivers"][it->first] = it->second->info();
    }

    return info;
}

std::shared_ptr<api::stream_t>
app_t::enqueue(const api::event_t& event, const std::shared_ptr<api::stream_t>& upstream) {
    return m_engine->enqueue(event, upstream);
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
