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
        s->rd->bind(std::bind(&client_t::on_message, this, std::placeholders::_1),
                    std::bind(&client_t::on_error, this, std::placeholders::_1));

        s->wr->bind(std::bind(&client_t::on_error, this, std::placeholders::_1));

        m_session = std::make_shared<session_t>(std::move(s));
    }

    ~client_t() {
        m_session->detach();
    }

    void
    bind(std::function<void(const std::error_code&)> error_handler) {
        m_error_handler = error_handler;
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
        m_session->detach();

        if (m_error_handler) {
            m_error_handler(ec);
        }
    }

private:
    std::shared_ptr<session_t> m_session;
    std::atomic<uint64_t> m_next_channel;

    std::function<void(const std::error_code&)> m_error_handler;
};

class service_resolver_t {
    COCAINE_DECLARE_NONCOPYABLE(service_resolver_t)

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
        m_error_handler = error_handler;

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

        m_callback = nullptr;
        m_error_handler = nullptr;
    }

private:
    class resolve_dispatch_t :
        public implements<io::locator::resolve::drain_type>
    {
    public:
        resolve_dispatch_t(context_t &context,
                           service_resolver_t &resolver,
                           const std::shared_ptr<client_t>& locator):
            implements<io::locator::resolve::drain_type>(context, "resolve"),
            m_resolver(resolver),
            m_locator(locator)
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
                m_resolver.m_reactor.post(std::bind(m_resolver.m_error_handler, e.code()));
                m_resolver.m_resolve_upstream->revoke();
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
            // TODO: Provide some useful error_code.
            m_resolver.m_reactor.post(std::bind(m_resolver.m_error_handler, std::error_code()));
            m_resolver.m_resolve_upstream->revoke();
        }

        void
        on_choke() {
            // Empty.
        }

    private:
        service_resolver_t& m_resolver;

        // The dispatch keeps locator client alive.
        // When the request will be finished, the dispatch will be removed from the client.
        const std::shared_ptr<client_t> m_locator;
    };

    void
    on_locator_connected(const std::shared_ptr<io::socket<io::tcp>>& socket) {
        m_connector.reset();

        auto channel = std::make_unique<io::channel<io::socket<io::tcp>>>(m_reactor, socket);
        auto locator = std::make_shared<client_t>(std::move(channel));

        locator->bind(m_error_handler);

        m_resolve_upstream = locator->call<cocaine::io::locator::resolve>(
            std::make_shared<resolve_dispatch_t>(m_context, *this, locator),
            m_service
        );
    }

    void
    on_service_connected(const std::shared_ptr<io::socket<io::tcp>>& socket) {
        m_connector.reset();

        auto channel = std::make_unique<io::channel<io::socket<io::tcp>>>(m_reactor, socket);
        auto client = std::make_shared<client_t>(std::move(channel));

        m_reactor.post(std::bind(m_callback, client));
    }

    void
    on_connection_error(const std::error_code& ec) {
        m_connector.reset();
        m_reactor.post(std::bind(m_error_handler, ec));
    }

private:
    context_t& m_context;
    io::reactor_t& m_reactor;

    std::vector<endpoint_type> m_locator;
    std::string m_service;

    std::shared_ptr<io::connector<io::socket<io::tcp>>> m_connector;
    std::shared_ptr<upstream_t> m_resolve_upstream;

    std::function<void(const std::shared_ptr<client_t>&)> m_callback;
    std::function<void(const std::error_code&)> m_error_handler;
};

} // namespace cocaine

#endif // COCAINE_CLIENT_HPP
