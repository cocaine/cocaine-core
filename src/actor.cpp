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

#include "cocaine/actor.hpp"

#include "cocaine/asio/acceptor.hpp"
#include "cocaine/asio/connector.hpp"
#include "cocaine/asio/socket.hpp"
#include "cocaine/asio/tcp.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"
#include "cocaine/messages.hpp"

#include "cocaine/rpc/channel.hpp"

#include <boost/bind.hpp>

using namespace cocaine;
using namespace cocaine::io;

namespace {
    struct upstream_t:
        public api::stream_t
    {
        upstream_t(const std::shared_ptr<channel<socket<tcp>>>& channel):
            m_channel(channel)
        { }

        virtual
        void
        write(const char * chunk,
              size_t size)
        {
            m_channel->wr->write<rpc::chunk>(0ULL, std::string(chunk, size));
        }

        virtual
        void
        error(error_code code,
              const std::string& reason)
        {
            m_channel->wr->write<rpc::error>(0ULL, static_cast<int>(code), reason);
        }

        virtual
        void
        close() {
            m_channel->wr->write<rpc::choke>(0ULL);
        }

    private:
        std::shared_ptr<channel<socket<tcp>>> m_channel;
    };
}

reactor_t::reactor_t(context_t& context,
                     const std::string& name,
                     const Json::Value& args):
    category_type(context, name, args),
    m_context(context),
    m_log(new logging::log_t(m_context, name)),
    m_terminate(m_service.loop())
{
    tcp::endpoint endpoint;

    if(args["port"].empty()) {
        endpoint = tcp::endpoint("127.0.0.1", 0);
    } else {
        endpoint = tcp::endpoint("127.0.0.1", args["port"].asUInt());
    }

    try {
        m_connector.reset(new connector<acceptor<tcp>>(
            m_service,
            std::unique_ptr<acceptor<tcp>>(new acceptor<tcp>(endpoint)))
        );
    } catch(const cocaine::io_error_t& e) {
        throw configuration_error_t(
            "unable to bind at '%s' - %s - %s",
            endpoint,
            e.what(),
            e.describe()
        );
    }

    COCAINE_LOG_INFO(m_log, "listening on '%s'", m_connector->endpoint());

    // NOTE: Register this service with the central dispatch, which will later
    // be published as a service itself for dynamic endpoint resolution.
    // m_context.dispatch().bind(name, endpoint);

    auto callback = std::bind(
        &reactor_t::on_connection,
        this,
        std::placeholders::_1
    );

    m_connector->bind(callback);

    m_terminate.set<reactor_t, &reactor_t::on_terminate>(this);
    m_terminate.start();
}

reactor_t::~reactor_t() {
    // m_context.dispatch().unbind(name);
}

void
reactor_t::run() {
    BOOST_ASSERT(!m_thread);

    // NOTE: For some reason, std::bind cannot resolve overloaded ambiguity
    // here while boost::bind can, so stick to it for now.
    auto runnable = boost::bind(
        &io::service_t::run,
        &m_service
    );

    m_thread.reset(new std::thread(runnable));
}

void
reactor_t::terminate() {
    BOOST_ASSERT(m_thread);

    m_terminate.send();

    m_thread->join();
    m_thread.reset();
}

void
reactor_t::on_connection(const std::shared_ptr<io::socket<tcp>>& socket_) {
    auto channel_ = std::make_shared<channel<io::socket<tcp>>>(m_service, socket_);

    channel_->rd->bind(
        std::bind(&reactor_t::on_message, this, channel_, std::placeholders::_1),
        std::bind(&reactor_t::on_disconnect, this, channel_, std::placeholders::_1)
    );

    channel_->wr->bind(
        std::bind(&reactor_t::on_disconnect, this, channel_, std::placeholders::_1)
    );

    m_channels.insert(channel_);
}

void
reactor_t::on_message(const std::shared_ptr<channel<io::socket<tcp>>>& channel_,
                      const message_t& message)
{
    slot_map_t::const_iterator slot = m_slots.find(message.id());

    if(slot == m_slots.end()) {
        COCAINE_LOG_WARNING(m_log, "dropping an unknown type %d message", message.id());
        return;
    }

    auto upstream = std::make_shared<upstream_t>(channel_);

    try {
        (*slot->second)(upstream, message.args());
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(
            m_log,
            "unable to process type %d message - %s",
            message.id(),
            e.what()
        );

        upstream->error(invocation_error, e.what());
        upstream->close();
    }
}

void
reactor_t::on_disconnect(const std::shared_ptr<channel<io::socket<tcp>>>& channel_,
                         const std::error_code& /* ec */)
{
    channel_->rd->unbind();
    channel_->wr->unbind();

    m_channels.erase(channel_);
}

void
reactor_t::on_terminate(ev::async&, int) {
    m_service.stop();
}

