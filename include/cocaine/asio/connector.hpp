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

#ifndef COCAINE_IO_CONNECTOR_HPP
#define COCAINE_IO_CONNECTOR_HPP

#include "cocaine/asio/reactor.hpp"
#include "cocaine/asio/acceptor.hpp"

namespace cocaine { namespace io {

template<class Acceptor>
struct connector;

template<class Medium>
struct connector<acceptor<Medium>> {
    COCAINE_DECLARE_NONCOPYABLE(connector)

    typedef acceptor<Medium> acceptor_type;
    typedef typename acceptor_type::endpoint_type endpoint_type;
    typedef typename acceptor_type::socket_type socket_type;

    connector(reactor_t& reactor, endpoint_type endpoint):
        m_acceptor(new acceptor_type(endpoint)),
        m_acceptor_watcher(reactor.native())
    {
        m_acceptor_watcher.set<connector, &connector::on_event>(this);
    }

    connector(reactor_t& reactor, std::unique_ptr<acceptor_type>&& acceptor):
        m_acceptor(std::move(acceptor)),
        m_acceptor_watcher(reactor.native())
    {
        m_acceptor_watcher.set<connector, &connector::on_event>(this);
    }

    template<class ConnectionHandler>
    void
    bind(ConnectionHandler handler) {
        if(!m_acceptor_watcher.is_active()) {
            m_acceptor_watcher.start(m_acceptor->fd(), ev::READ);
        }

        m_callback = handler;
    }

    void
    unbind() {
        if(m_acceptor_watcher.is_active()) {
            m_acceptor_watcher.stop();
        }

        m_callback = nullptr;
    }

    endpoint_type
    endpoint() const {
        return m_acceptor->local_endpoint();
    }

private:
    void
    on_event(ev::io& /* io */, int /* revents */) {
        std::error_code ec;

        const std::shared_ptr<socket_type>& socket = m_acceptor->accept(ec);

        if(!socket) {
            return;
        }

        m_callback(socket);
    }

private:
    // NOTE: It doesn't make sense to accept a connection from multiple queues,
    // so keep at most one reference to the acceptor.
    const std::unique_ptr<acceptor_type> m_acceptor;

    // Acceptor poll object.
    ev::io m_acceptor_watcher;

    // Acceptor connection callback.
    std::function<
        void(const std::shared_ptr<socket_type>&)
    > m_callback;
};

template<class Medium>
struct connector<socket<Medium>> {
    COCAINE_DECLARE_NONCOPYABLE(connector)

    typedef socket<Medium> socket_type;
    typedef typename socket_type::endpoint_type endpoint_type;

    connector(reactor_t& reactor, endpoint_type endpoint):
        m_endpoints(1, endpoint),
        m_next(0),
        m_reactor(reactor),
        m_socket_watcher(reactor.native())
    {
        m_socket_watcher.set<connector, &connector::on_event>(this);
    }

    connector(reactor_t& reactor, const std::vector<endpoint_type>& endpoints):
        m_endpoints(endpoints),
        m_next(0),
        m_reactor(reactor),
        m_socket_watcher(reactor.native())
    {
        m_socket_watcher.set<connector, &connector::on_event>(this);
    }

    ~connector() {
        unbind();
    }

    template<class ConnectionHandler, class ErrorHandler>
    void
    bind(ConnectionHandler&& connection_handler, ErrorHandler&& error_handler) {
        connect();

        m_callback = connection_handler;
        m_error_handler = error_handler;
    }

    void
    unbind() {
        if(m_socket_watcher.is_active()) {
            m_socket_watcher.stop();
        }

        m_callback = nullptr;
        m_error_handler = nullptr;
    }

    endpoint_type
    endpoint() const {
        return m_socket->local_endpoint();
    }

private:
    void
    connect() {
        if(m_next >= m_endpoints.size()) {
            m_reactor.post(std::bind(m_error_handler, m_last_error));
            return;
        }

        m_socket = std::make_shared<socket_type>();
        auto current = m_next++;

        if(::connect(m_socket->fd(), m_endpoints[current].data(), m_endpoints[current].size()) != 0 &&
           errno != EINPROGRESS)
        {
            m_last_error = std::error_code(errno, std::system_category());
            connect();
        }

        if(!m_socket_watcher.is_active()) {
            m_socket_watcher.start(m_socket->fd(), ev::WRITE);
        }
    }

    void
    on_event(ev::io& /* io */, int /* revents */) {
        int errc;
        socklen_t errc_len = sizeof(errc);

        if(getsockopt(m_socket->fd(), SOL_SOCKET, SO_ERROR, &errc, &errc_len) != 0) {
            m_last_error = std::error_code(errno, std::system_category());
        } else if(errc == EINPROGRESS) {
            return;
        } else if(errc != 0) {
            m_last_error = std::error_code(errc, std::system_category());
        } else {
            m_socket_watcher.stop();
            m_reactor.post(std::bind(m_callback, m_socket));
            return;
        }

        m_socket_watcher.stop();
        connect();
    }

private:
    std::vector<endpoint_type> m_endpoints;
    size_t m_next;

    std::shared_ptr<socket_type> m_socket;
    std::error_code m_last_error;

    reactor_t& m_reactor;
    ev::io m_socket_watcher;

    std::function<void(const std::shared_ptr<socket_type>&)> m_callback;
    std::function<void(const std::error_code&)> m_error_handler;
};

}} // namespace cocaine::io

#endif
