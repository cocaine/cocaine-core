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

#include "cocaine/dispatch.hpp"

#include "cocaine/context.hpp"
#include "cocaine/logging.hpp"

#include "cocaine/rpc/message.hpp"

using namespace cocaine;

dispatch_t::dispatch_t(context_t& context, const std::string& name):
    m_log(new logging::log_t(context, name))
{ }

dispatch_t::~dispatch_t() {
    // Empty.
}

void
dispatch_t::invoke(const io::message_t& message, const api::stream_ptr_t& upstream) {
    slot_map_t::mapped_type slot;

    {
        std::unique_lock<std::mutex> lock(m_mutex);

        slot_map_t::const_iterator it = m_slots.find(message.id());

        if(it == m_slots.end()) {
            COCAINE_LOG_WARNING(
                m_log,
                "dropping an unknown type %d: %s message",
                message.id(),
                message.args()
            );

            lock.unlock();

            upstream->error(invocation_error, "unknown message type");
            upstream->close();

            return;
        }

        slot = it->second;
    }

    COCAINE_LOG_DEBUG(
        m_log,
        "processing type %d: %s message using slot '%s'",
        message.id(),
        message.args(),
        slot->name()
    );

    try {
        (*slot)(message.args(), upstream);
    } catch(const std::exception& e) {
        COCAINE_LOG_ERROR(
            m_log,
            "unable to process type %d: %s message using slot '%s' - %s",
            message.id(),
            message.args(),
            slot->name(),
            e.what()
        );

        upstream->error(invocation_error, e.what());
        upstream->close();
    }
}

std::map<int, std::string>
dispatch_t::describe() {
    std::map<int, std::string> result;
    std::lock_guard<std::mutex> lock(m_mutex);

    for(auto it = m_slots.begin(); it != m_slots.end(); ++it) {
        result[it->first] = it->second->name();
    }

    return result;
}
