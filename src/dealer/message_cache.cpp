/*
    Copyright (c) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
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

#include <algorithm>
#include <stdexcept>
#include <uuid/uuid.h>
#include <map>
#include <cstring>
#include <algorithm>

#include <boost/bind.hpp>
#include <boost/tokenizer.hpp>
#include <boost/progress.hpp>

#include "cocaine/dealer/core/message_cache.hpp"
#include "cocaine/dealer/utils/error.hpp"
#include "cocaine/dealer/utils/progress_timer.hpp"

namespace cocaine {
namespace dealer {

message_cache_t::message_cache_t(const boost::shared_ptr<context_t>& ctx,
							 bool logging_enabled) :
	dealer_object_t(ctx, logging_enabled)
{
	m_type = config()->message_cache_type();
	m_new_messages.reset(new message_queue_t);
}

message_cache_t::~message_cache_t() {
}

message_cache_t::message_queue_ptr_t
message_cache_t::new_messages() {
	if (!m_new_messages) {
		std::string error_str = "new messages queue object is empty at ";
		error_str += std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_str);
	}

	return m_new_messages;
}

void
message_cache_t::enqueue_with_priority(const boost::shared_ptr<message_iface>& message) {
	boost::mutex::scoped_lock lock(m_mutex);
	m_new_messages->push_front(message);
}

void
message_cache_t::enqueue(const boost::shared_ptr<message_iface>& message) {
	boost::mutex::scoped_lock lock(m_mutex);
	m_new_messages->push_back(message);
}

void
message_cache_t::append_message_queue(message_queue_ptr_t queue) {
	boost::mutex::scoped_lock lock(m_mutex);

	// validate new queue
	if (!queue || queue->empty()) {
		return;
	}

	// append messages
	m_new_messages->insert(m_new_messages->end(), queue->begin(), queue->end());
}

boost::shared_ptr<message_iface>
message_cache_t::get_new_message() {
	boost::mutex::scoped_lock lock(m_mutex);
	return m_new_messages->front();
}

size_t
message_cache_t::new_messages_count() {
	boost::mutex::scoped_lock lock(m_mutex);
	return m_new_messages->size();
}

size_t
message_cache_t::sent_messages_count() {
	boost::mutex::scoped_lock lock(m_mutex);

	size_t sent_messages_count = 0;

	route_sent_messages_map_t::const_iterator it = m_sent_messages.begin();
	for (; it != m_sent_messages.end(); ++it) {
		sent_messages_count += it->second.size();
	}

	return sent_messages_count;
}

bool
message_cache_t::get_sent_message(const std::string& route,
								const std::string& uuid,
								boost::shared_ptr<message_iface>& message) {

	boost::mutex::scoped_lock lock(m_mutex);

	route_sent_messages_map_t::const_iterator it = m_sent_messages.find(route);

	if (it == m_sent_messages.end()) {
		return false;
	}

	const sent_messages_map_t& msg_map = it->second;
	sent_messages_map_t::const_iterator mit = msg_map.find(uuid);

	if (mit == msg_map.end()) {
		return false;
	}

	assert(mit->second);
	message = mit->second;

	return true;
}

void
message_cache_t::move_new_message_to_sent(const std::string& route) {
	boost::mutex::scoped_lock lock(m_mutex);

	boost::shared_ptr<message_iface> msg = m_new_messages->front();
	assert(msg);

	route_sent_messages_map_t::iterator it = m_sent_messages.find(route);
	if (it == m_sent_messages.end()) {
		sent_messages_map_t msg_map;
		msg_map.insert(std::make_pair(msg->uuid(), msg));
		m_sent_messages[route] = msg_map;
	}
	else {
		it->second.insert(std::make_pair(msg->uuid(), msg));
	}

	m_new_messages->pop_front();
}

void
message_cache_t::move_sent_message_to_new(const std::string& route, const std::string& uuid) {
	boost::mutex::scoped_lock lock(m_mutex);

	route_sent_messages_map_t::iterator it = m_sent_messages.find(route);

	if (it == m_sent_messages.end()) {
		return;
	}

	sent_messages_map_t& msg_map = it->second;
	sent_messages_map_t::iterator mit = msg_map.find(uuid);

	if (mit == msg_map.end()) {
		return;
	}

	boost::shared_ptr<message_iface> msg = mit->second;

	if (!msg) {
		throw internal_error("empty cached message object at " + std::string(BOOST_CURRENT_FUNCTION));
	}

	msg_map.erase(mit);

	msg->mark_as_sent(false);
	msg->set_ack_received(false);

	m_new_messages->push_back(msg);
}

void
message_cache_t::move_sent_message_to_new_front(const std::string& route, const std::string& uuid) {
	boost::mutex::scoped_lock lock(m_mutex);

	route_sent_messages_map_t::iterator it = m_sent_messages.find(route);

	if (it == m_sent_messages.end()) {
		return;
	}

	sent_messages_map_t& msg_map = it->second;
	sent_messages_map_t::iterator mit = msg_map.find(uuid);

	if (mit == msg_map.end()) {
		return;
	}

	boost::shared_ptr<message_iface> msg = mit->second;

	if (!msg) {
		throw internal_error("empty cached message object at " + std::string(BOOST_CURRENT_FUNCTION));
	}

	msg_map.erase(mit);

	msg->mark_as_sent(false);
	msg->set_ack_received(false);

	m_new_messages->push_front(msg);
}

void
message_cache_t::remove_message_from_cache(const std::string& route, const std::string& uuid) {
	boost::mutex::scoped_lock lock(m_mutex);

	route_sent_messages_map_t::iterator it = m_sent_messages.find(route);

	if (it == m_sent_messages.end()) {
		return;
	}

	sent_messages_map_t& msg_map = it->second;
	sent_messages_map_t::iterator mit = msg_map.find(uuid);

	if (mit == msg_map.end()) {
		return;
	}

	msg_map.erase(mit);
}

void
message_cache_t::make_all_messages_new() {
	boost::mutex::scoped_lock lock(m_mutex);

	route_sent_messages_map_t::iterator it = m_sent_messages.begin();
	for (; it != m_sent_messages.end(); ++it) {

		sent_messages_map_t& msg_map = it->second;
		sent_messages_map_t::iterator mit = msg_map.begin();

		for (; mit != msg_map.end(); ++mit) {

			if (!mit->second) {
				throw internal_error("empty cached message object at " + std::string(BOOST_CURRENT_FUNCTION));
			}

			mit->second->mark_as_sent(false);
			mit->second->set_ack_received(false);
			m_new_messages->push_front(mit->second);
		}

		msg_map.clear();
	}
}

void
message_cache_t::make_all_messages_new_for_route(const std::string& route) {
	boost::mutex::scoped_lock lock(m_mutex);

	route_sent_messages_map_t::iterator it = m_sent_messages.find(route);
	if (it == m_sent_messages.end()) {
		return;
	}

	sent_messages_map_t& msg_map = it->second;
	sent_messages_map_t::iterator mit = msg_map.begin();

	if (msg_map.empty()) {
		return;
	}
	
	for (; mit != msg_map.end(); ++mit) {

		if (!mit->second) {
			throw internal_error("empty cached message object at " + std::string(BOOST_CURRENT_FUNCTION));
		}

		mit->second->mark_as_sent(false);
		mit->second->set_ack_received(false);
		m_new_messages->push_front(mit->second);
	}

	msg_map.clear();
}

bool
message_cache_t::is_message_expired(cached_message_ptr_t msg) {
	return msg->is_expired();
}

void
message_cache_t::get_expired_messages(message_queue_t& expired_messages) {
	boost::mutex::scoped_lock lock(m_mutex);

	assert(m_new_messages);

	// remove expired from sent
	route_sent_messages_map_t::iterator it = m_sent_messages.begin();
	for (; it != m_sent_messages.end(); ++it) {

		sent_messages_map_t& msg_map = it->second;
		sent_messages_map_t::iterator mit = msg_map.begin();

		while (mit != msg_map.end()) {
			// get single sent message
			boost::shared_ptr<message_iface> msg = mit->second;
			assert(msg);

			// remove expired messages
			if (msg->is_expired()) {
				expired_messages.push_back(msg);
				msg_map.erase(mit++);
			}
			else {
				++mit;
			}
		}
	}

	// remove expired from new
	message_queue_t::iterator it2 = m_new_messages->begin();
	while (it2 != m_new_messages->end()) {
		// get single pending message
		boost::shared_ptr<message_iface> msg = *it2;
		assert(msg);

		// remove expired messages
		if (msg->is_expired()) {
			expired_messages.push_back(msg);
		}

		++it2;
	}

	m_new_messages->erase(std::remove_if(m_new_messages->begin(),
										 m_new_messages->end(),
										 &message_cache_t::is_message_expired),
										 m_new_messages->end());
}

} // namespace dealer
} // namespace cocaine
