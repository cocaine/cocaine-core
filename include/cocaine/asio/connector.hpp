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

#ifndef COCAINE_ASIO_CONNECTOR_HPP
#define COCAINE_ASIO_CONNECTOR_HPP

#include "cocaine/asio/service.hpp"

#include <functional>

namespace cocaine { namespace io {

template<class Acceptor>
struct connector:
    boost::noncopyable
{
    typedef Acceptor acceptor_type;
    typedef typename acceptor_type::endpoint_type endpoint_type;
    typedef typename acceptor_type::pipe_type pipe_type;

    connector(service_t& service,
              endpoint_type endpoint):
        m_acceptor(new acceptor_type(endpoint)),
        m_acceptor_watcher(service.loop())
    {
        m_acceptor_watcher.set<connector, &connector::on_event>(this);
    }

    connector(service_t& service,
              std::unique_ptr<acceptor_type>&& acceptor):
        m_acceptor(std::move(acceptor)),
        m_acceptor_watcher(service.loop())
    {
        m_acceptor_watcher.set<connector, &connector::on_event>(this);
    }

    template<class Callback>
    void
    bind(Callback callback) {
        m_callback = callback;
        m_acceptor_watcher.start(m_acceptor->fd(), ev::READ);
    }

    void
    unbind() {
        m_callback = nullptr;

        if(m_acceptor_watcher.is_active()) {
            m_acceptor_watcher.stop();
        }
    }

private:
    void
    on_event(ev::io& /* io */, int /* revents */) {
        const std::shared_ptr<pipe_type>& pipe = m_acceptor->accept();

        if(!pipe) {
            return;
        }

        m_callback(pipe);
    }

private:
    // NOTE: It doesn't make sense to accept a connection from multiple queues,
    // so keep at most one reference to the acceptor.
    const std::unique_ptr<acceptor_type> m_acceptor;

    // Acceptor poll object.
    ev::io m_acceptor_watcher;

    // Acceptor connection callback.
    std::function<
        void(const std::shared_ptr<pipe_type>&)
    > m_callback;
};

}} // namespace cocaine::io

#endif
