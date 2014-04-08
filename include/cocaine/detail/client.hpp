/*
    Copyright (c) 2013-2014 Andrey Goryachev <andrey.goryachev@gmail.com>
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

#ifndef COCAINE_CLIENT_HPP
#define COCAINE_CLIENT_HPP

#include "cocaine/idl/streaming.hpp"
#include "cocaine/idl/locator.hpp"

#include "cocaine/rpc/upstream.hpp"
#include "cocaine/rpc/protocol.hpp"

#include "cocaine/dispatch.hpp"

#include "cocaine/asio/connector.hpp"
#include "cocaine/asio/resolver.hpp"

#include "cocaine/detail/atomic.hpp"
#include "cocaine/memory.hpp"

#include <boost/variant.hpp>

#include <type_traits>
#include <utility>

namespace cocaine {

class client_t {
    COCAINE_DECLARE_NONCOPYABLE(client_t)

public:
    typedef cocaine::io::channel<cocaine::io::socket<cocaine::io::tcp>>
            stream_t;

    client_t(std::unique_ptr<stream_t>&& s):
        m_next_channel(1)
    {
        m_on_error = std::make_shared<std::function<void(const std::error_code&)>>(
            std::bind(&client_t::on_error, this, std::placeholders::_1)
        );

        s->rd->bind(std::bind(&client_t::on_message, this, std::placeholders::_1),
                    io::make_task(m_on_error));

        s->wr->bind(io::make_task(m_on_error));

        m_session = std::make_shared<session_t>(std::move(s));
    }

    ~client_t() {
        unbind();
    }

    void
    bind(std::function<void(const std::error_code&)> error_handler) {
        m_error_handler = error_handler;
    }

    void
    unbind() {
        auto session = std::move(m_session);
        if(session) {
            // The client can be destroyed here.
            session->detach();
        }
    }

    template<class Event, class... Args>
    std::shared_ptr<upstream_t>
    call(const std::shared_ptr<io::dispatch_t>& handler, Args&&... args) {
        auto dispatch = std::is_same<typename io::event_traits<Event>::drain_type, void>::value ?
                        std::shared_ptr<io::dispatch_t>() :
                        handler;
        auto upstream = m_session->invoke(m_next_channel++, dispatch);
        upstream->template send<Event>(std::forward<Args>(args)...);
        return upstream;
    }

private:
    void
    on_message(const cocaine::io::message_t& message) {
        m_session->invoke(message);
    }

    void
    on_error(const std::error_code& ec) {
        auto error_handler = m_error_handler;

        unbind();

        if (error_handler) {
            error_handler(ec);
        }
    }

private:
    std::shared_ptr<session_t> m_session;
    std::atomic<uint64_t> m_next_channel;

    std::function<void(const std::error_code&)> m_error_handler;
    std::shared_ptr<std::function<void(const std::error_code&)>> m_on_error;
};

class service_resolver_t {
    COCAINE_DECLARE_NONCOPYABLE(service_resolver_t)

    typedef std::function<void(const std::error_code&)> error_handler_t;

public:
    typedef io::tcp::endpoint endpoint_type;

    service_resolver_t(context_t& context,
                       io::reactor_t& reactor,
                       const std::vector<endpoint_type>& locator,
                       const std::string& service):
        m_context(context),
        m_reactor(reactor),
        m_locator(locator),
        m_service(service)
    { }

    ~service_resolver_t() {
        unbind();
    }

    template<class Handler, class ErrorHandler>
    void
    bind(Handler callback, ErrorHandler error_handler) {
        m_callback = callback;
        m_error_handler = std::make_shared<error_handler_t>(error_handler);

        using namespace std::placeholders;

        m_connector = std::make_shared<io::connector<io::socket<io::tcp>>>(m_reactor, m_locator);
        m_connector->bind(std::bind(&service_resolver_t::on_locator_connected, this, _1),
                          std::bind(&service_resolver_t::on_connection_error, this, _1));
    }

    void
    unbind() {
        m_connector.reset();

        if(m_resolve_upstream) {
            m_resolve_upstream->revoke();
            m_resolve_upstream.reset();
        }

        m_locator_client.reset();

        m_callback = nullptr;
        m_error_handler.reset();
    }

private:
    class resolve_dispatch_t :
        public implements<io::locator::resolve::drain_type>
    {
    public:
        resolve_dispatch_t(service_resolver_t &resolver):
            implements<io::locator::resolve::drain_type>("resolve"),
            m_resolver(resolver)
        {
            using namespace std::placeholders;

            typedef io::streaming<io::locator::resolve::value_type> stream_type;

            on<stream_type::chunk>(std::bind(&resolve_dispatch_t::on_write, this, _1, _2, _3));
            on<stream_type::error>(std::bind(&resolve_dispatch_t::on_error, this, _1, _2));
            on<stream_type::choke>(std::bind(&resolve_dispatch_t::on_choke, this));
        }

    private:
        void
        on_write(const io::locator::resolve::endpoint_tuple_type& endpoint,
                 unsigned int,
                 const io::dispatch_graph_t&)
        {
            std::vector<io::tcp::endpoint> endpoints;

            try {
                endpoints = io::resolver<io::tcp>::query(std::get<0>(endpoint),
                                                         std::get<1>(endpoint));
            } catch(const std::system_error& e) {
                auto &resolver = m_resolver;
                resolver.m_resolve_upstream->revoke();
                resolver.m_reactor.post(
                    std::bind(io::make_task(resolver.m_error_handler), e.code())
                );
                return;
            }

            m_resolver.m_connector = std::make_shared<io::connector<io::socket<io::tcp>>>(
                m_resolver.m_reactor,
                endpoints
            );

            using namespace std::placeholders;

            m_resolver.m_connector->bind(
                std::bind(&service_resolver_t::on_service_connected, &m_resolver, _1),
                std::bind(&service_resolver_t::on_connection_error, &m_resolver, _1)
            );

            m_resolver.m_resolve_upstream->revoke();
        }

        void
        on_error(int, const std::string&) {
            auto &resolver = m_resolver;
            resolver.m_resolve_upstream->revoke();
            // TODO: Provide some useful error_code.
            resolver.m_reactor.post(
                std::bind(io::make_task(resolver.m_error_handler), std::error_code())
            );
        }

        void
        on_choke() {
            auto &resolver = m_resolver;
            resolver.m_resolve_upstream->revoke();
            resolver.m_reactor.post(
                std::bind(io::make_task(resolver.m_error_handler), std::error_code())
            );

        }

    private:
        service_resolver_t &m_resolver;
    };

    void
    on_locator_connected(const std::shared_ptr<io::socket<io::tcp>>& socket) {
        m_connector.reset();

        auto channel = std::make_unique<io::channel<io::socket<io::tcp>>>(m_reactor, socket);
        m_locator_client = std::make_shared<client_t>(std::move(channel));

        m_locator_client->bind(*m_error_handler);

        m_resolve_upstream = m_locator_client->call<cocaine::io::locator::resolve>(
            std::make_shared<resolve_dispatch_t>(*this),
            m_service
        );
    }

    void
    on_service_connected(const std::shared_ptr<io::socket<io::tcp>>& socket) {
        m_connector.reset();

        auto channel = std::make_unique<io::channel<io::socket<io::tcp>>>(m_reactor, socket);
        auto client = std::make_shared<client_t>(std::move(channel));

        auto callback = m_callback;
        callback(client);
    }

    void
    on_connection_error(const std::error_code& ec) {
        m_connector.reset();

        auto error_handler = m_error_handler;
        (*error_handler)(ec);
    }

private:
    context_t& m_context;
    io::reactor_t& m_reactor;

    std::vector<endpoint_type> m_locator;
    std::string m_service;

    std::shared_ptr<io::connector<io::socket<io::tcp>>> m_connector;
    std::shared_ptr<client_t> m_locator_client;
    std::shared_ptr<upstream_t> m_resolve_upstream;

    std::function<void(const std::shared_ptr<client_t>&)> m_callback;
    std::shared_ptr<error_handler_t> m_error_handler;
};

template<class T>
class proxy_dispatch :
    public implements<io::streaming_tag<T>>
{
    typedef boost::variant<std::error_code, T> result_type;
    typedef io::streaming<T> protocol_type;

public:
    proxy_dispatch(const std::function<void(result_type)>& callback,
                   const std::string& name = ""):
        implements<io::streaming_tag<T>>(name),
        m_callback(callback)
    {
        using namespace std::placeholders;

        this->template on<typename protocol_type::chunk>(std::bind(&proxy_dispatch::on_write, this, _1));
        this->template on<typename protocol_type::error>(std::bind(&proxy_dispatch::on_error, this, _1, _2));
        this->template on<typename protocol_type::choke>(std::bind(&proxy_dispatch::on_choke, this));
    }

private:
    void
    on_write(const T& result) {
        if(m_callback) {
            m_callback(result);
            m_callback = nullptr;
        }
    }

    void
    on_error(int, const std::string&) {
        if(m_callback) {
            m_callback(std::error_code());
            m_callback = nullptr;
        }
    }

    void
    on_choke() {
        if(m_callback) {
            m_callback(std::error_code());
            m_callback = nullptr;
        }
    }

private:
    std::function<void(result_type)> m_callback;
};

template<class... Args>
class proxy_dispatch<std::tuple<Args...>> :
    public implements<io::streaming_tag<std::tuple<Args...>>>
{
    typedef std::tuple<Args...> tuple_type;
    typedef boost::variant<std::error_code, tuple_type> result_type;
    typedef io::streaming<tuple_type> protocol_type;

    struct write_handler {
        typedef void result_type;

        proxy_dispatch<std::tuple<Args...>> *dispatch;

        void
        operator()(const Args&... args) const {
            dispatch->on_write(args...);
        }
    };

public:
    proxy_dispatch(const std::function<void(result_type)>& callback,
                   const std::string& name = ""):
        implements<io::streaming_tag<tuple_type>>(name),
        m_callback(callback)
    {
        using namespace std::placeholders;

        this->template on<typename protocol_type::chunk>(write_handler {this});
        this->template on<typename protocol_type::error>(
            std::bind(&proxy_dispatch::on_error, this, _1, _2)
        );
        this->template on<typename protocol_type::choke>(std::bind(&proxy_dispatch::on_choke, this));
    }

private:
    void
    on_write(const Args&... args) {
        if(m_callback) {
            auto callback = m_callback;
            m_callback = nullptr;
            callback(tuple_type(args...));
        }
    }

    void
    on_error(int, const std::string&) {
        if(m_callback) {
            auto callback = m_callback;
            m_callback = nullptr;
            callback(std::error_code());
        }
    }

    void
    on_choke() {
        if(m_callback) {
            auto callback = m_callback;
            m_callback = nullptr;
            callback(std::error_code());
        }
    }

private:
    std::function<void(result_type)> m_callback;
};

template<class T, class... Args>
std::shared_ptr<proxy_dispatch<T>>
make_proxy(Args&&... args) {
    return std::make_shared<proxy_dispatch<T>>(std::forward<Args>(args)...);
}

} // namespace cocaine

#endif // COCAINE_CLIENT_HPP
