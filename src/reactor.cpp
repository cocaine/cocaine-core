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

#include "cocaine/reactor.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/asio/acceptor.hpp"
#include "cocaine/asio/connector.hpp"
#include "cocaine/asio/pipe.hpp"

using namespace cocaine;
using namespace cocaine::io;

reactor_t::reactor_t(context_t& context,
                     const std::string& name,
                     const Json::Value& args):
    category_type(context, name, args),
    m_context(context),
    m_log(new logging::log_t(m_context, name)),
    m_terminate(m_service.loop())
{
    if(args["listen"].empty() || !args["listen"].isArray()) {
        throw cocaine::error_t("no endpoints has been specified");
    }

    for(Json::Value::const_iterator it = args["listen"].begin();
        it != args["listen"].end();
        ++it)
    {
        std::string endpoint = (*it).asString();

        COCAINE_LOG_INFO(m_log, "listening on '%s'", endpoint);

        try {
            m_connector.reset(new connector<acceptor_t>(
                m_service,
                std::unique_ptr<acceptor_t>(new acceptor_t(endpoint)))
            );
        } catch(const cocaine::io_error_t& e) {
            throw configuration_error_t(
                "unable to bind at '%s' - %s - %s",
                endpoint,
                e.what(),
                e.describe()
            );
        }
    }

    m_connector->bind(
        boost::bind(&reactor_t::on_connection, this, _1)
    );

    m_terminate.set<reactor_t, &reactor_t::on_terminate>(this);
    m_terminate.start();
}

reactor_t::~reactor_t() {
    // Empty.
}

void
reactor_t::run() {
    BOOST_ASSERT(!m_thread);

    auto runnable = boost::bind(
        &io::service_t::run,
        &m_service
    );

    m_thread.reset(new boost::thread(runnable));
}

void
reactor_t::terminate() {
    BOOST_ASSERT(m_thread);

    m_terminate.send();

    m_thread->join();
    m_thread.reset();
}

void
reactor_t::on_connection(const boost::shared_ptr<pipe_t>& pipe) {
    COCAINE_LOG_INFO(m_log, "new client connection on fd: %s", pipe->fd());
    m_clients.insert(pipe);
}

void
reactor_t::on_message(const unique_id_t& client_id,
                      const message_t& message)
{
    slot_map_t::const_iterator slot = m_slots.find(message.id());

    if(slot == m_slots.end()) {
        COCAINE_LOG_WARNING(m_log, "dropping an unknown type %d message", message.id());
        return;
    }

    std::string response;

    try {
        response = (*slot->second)(message.args());
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(
            m_log,
            "unable to process type %d message - %s",
            message.id(),
            e.what()
        );

        return;
    }

    // if(!response.empty()) {
    //    m_channel.send_multipart(source, response);
    // }
}

void
reactor_t::on_terminate(ev::async&, int) {
    m_service.stop();
}

