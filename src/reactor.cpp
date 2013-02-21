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

#include "cocaine/asio/acceptor.hpp"
#include "cocaine/asio/connector.hpp"
#include "cocaine/asio/pipe.hpp"
#include "cocaine/asio/tcp.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include <boost/bind.hpp>

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
        auto endpoint = tcp::endpoint("127.0.0.1", (*it).asUInt());

        COCAINE_LOG_INFO(m_log, "listening on '%s'", endpoint);

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
    }

    m_connector->bind(
        std::bind(&reactor_t::on_connection, this, std::placeholders::_1)
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
reactor_t::on_connection(const std::shared_ptr<pipe<tcp>>& pipe_) {
    auto codec_ = std::make_shared<codec<pipe<tcp>>>(m_service, pipe_);

    codec_->rd->bind(
        std::bind(&reactor_t::on_message, this, codec_, std::placeholders::_1)
    );

    m_codecs.insert(codec_);
}

void
reactor_t::on_message(const std::shared_ptr<codec<pipe<tcp>>>& codec_,
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

    if(!response.empty()) {
        codec_->wr->stream()->write(response.data(), response.size());
    }
}

void
reactor_t::on_terminate(ev::async&, int) {
    m_service.stop();
}

