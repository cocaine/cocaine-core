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

#include <functional>

namespace cocaine { namespace io {

template<class Acceptor>
struct connector:
    boost::noncopyable
{
    typedef Acceptor acceptor_type;
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
    endpoint() {
        return m_acceptor->local_endpoint();
    }

private:
    void
    on_event(ev::io& /* io */, int /* revents */) {
        const std::shared_ptr<socket_type>& socket = m_acceptor->accept();

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

}} // namespace cocaine::io

#endif
