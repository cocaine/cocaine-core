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

#include "cocaine/dispatch.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/rpc/message.hpp"

using namespace cocaine;
using namespace cocaine::io;

dispatch_t::dispatch_t(context_t& context,
                       const std::string& name):
    m_context(context),
    m_log(new logging::log_t(context, name))
{ }

dispatch_t::~dispatch_t() {
    // Empty.
}

void
dispatch_t::dispatch(const message_t& message,
                     const api::stream_ptr_t& upstream) const
{
    slot_map_t::const_iterator slot = m_slots.find(message.id());

    if(slot == m_slots.end()) {
        COCAINE_LOG_WARNING(
            m_log,
            "dropping an unknown type %d message",
            message.id()
        );

        upstream->error(invocation_error, "unknown message type");
        upstream->close();
    }

    COCAINE_LOG_DEBUG(
        m_log,
        "processing type [%d, '%s'] message with arguments %s",
        message.id(),
        slot->second->describe(),
        message.args()
    );

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

