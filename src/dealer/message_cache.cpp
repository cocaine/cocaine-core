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

message_cache::message_cache(const boost::shared_ptr<context_t>& ctx,
							 bool logging_enabled) :
	dealer_object_t(ctx, logging_enabled)
{
	type_m = config()->message_cache_type();
	new_messages_m.reset(new message_queue_t);
}

message_cache::~message_cache() {
}

message_cache::message_queue_ptr_t
message_cache::new_messages() {
	if (!new_messages_m) {
		std::string error_str = "new messages queue object is empty at ";
		error_str += std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_str);
	}

	return new_messages_m;
}

void
message_cache::enqueue_with_priority(const boost::shared_ptr<message_iface>& message) {
	boost::mutex::scoped_lock lock(mutex_m);
	new_messages_m->push_front(message);
}

void
message_cache::enqueue(const boost::shared_ptr<message_iface>& message) {
	boost::mutex::scoped_lock lock(mutex_m);
	new_messages_m->push_back(message);
}

void
message_cache::append_message_queue(message_queue_ptr_t queue) {
	boost::mutex::scoped_lock lock(mutex_m);

	// validate new queue
	if (!queue || queue->empty()) {
		return;
	}

	// append messages
	new_messages_m->insert(new_messages_m->end(), queue->begin(), queue->end());
}

boost::shared_ptr<message_iface>
message_cache::get_new_message() {
	boost::mutex::scoped_lock lock(mutex_m);
	return new_messages_m->front();
}

size_t
message_cache::new_messages_count() {
	boost::mutex::scoped_lock lock(mutex_m);
	return new_messages_m->size();
}

size_t
message_cache::sent_messages_count() {
	boost::mutex::scoped_lock lock(mutex_m);

	size_t sent_messages_count = 0;

	route_sent_messages_map_t::const_iterator it = sent_messages_m.begin();
	for (; it != sent_messages_m.end(); ++it) {
		sent_messages_count += it->second.size();
	}

	return sent_messages_count;
}

bool
message_cache::get_sent_message(const std::string& route,
								const std::string& uuid,
								boost::shared_ptr<message_iface>& message) {

	boost::mutex::scoped_lock lock(mutex_m);

	route_sent_messages_map_t::const_iterator it = sent_messages_m.find(route);

	if (it == sent_messages_m.end()) {
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
message_cache::move_new_message_to_sent(const std::string& route) {
	boost::mutex::scoped_lock lock(mutex_m);

	boost::shared_ptr<message_iface> msg = new_messages_m->front();
	assert(msg);

	route_sent_messages_map_t::iterator it = sent_messages_m.find(route);
	if (it == sent_messages_m.end()) {
		sent_messages_map_t msg_map;
		msg_map.insert(std::make_pair(msg->uuid(), msg));
		sent_messages_m[route] = msg_map;
	}
	else {
		it->second.insert(std::make_pair(msg->uuid(), msg));
	}

	new_messages_m->pop_front();
}

void
message_cache::move_sent_message_to_new(const std::string& route, const std::string& uuid) {
	boost::mutex::scoped_lock lock(mutex_m);

	route_sent_messages_map_t::iterator it = sent_messages_m.find(route);

	if (it == sent_messages_m.end()) {
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

	new_messages_m->push_back(msg);
}

void
message_cache::move_sent_message_to_new_front(const std::string& route, const std::string& uuid) {
	boost::mutex::scoped_lock lock(mutex_m);

	route_sent_messages_map_t::iterator it = sent_messages_m.find(route);

	if (it == sent_messages_m.end()) {
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

	new_messages_m->push_front(msg);
}

void
message_cache::remove_message_from_cache(const std::string& route, const std::string& uuid) {
	boost::mutex::scoped_lock lock(mutex_m);

	route_sent_messages_map_t::iterator it = sent_messages_m.find(route);

	if (it == sent_messages_m.end()) {
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
message_cache::make_all_messages_new() {
	boost::mutex::scoped_lock lock(mutex_m);

	route_sent_messages_map_t::iterator it = sent_messages_m.begin();
	for (; it != sent_messages_m.end(); ++it) {

		sent_messages_map_t& msg_map = it->second;
		sent_messages_map_t::iterator mit = msg_map.begin();

		for (; mit != msg_map.end(); ++mit) {

			if (!mit->second) {
				throw internal_error("empty cached message object at " + std::string(BOOST_CURRENT_FUNCTION));
			}

			mit->second->mark_as_sent(false);
			mit->second->set_ack_received(false);
			new_messages_m->push_front(mit->second);
		}

		msg_map.clear();
	}
}

void
message_cache::make_all_messages_new_for_route(const std::string& route) {
	boost::mutex::scoped_lock lock(mutex_m);

	route_sent_messages_map_t::iterator it = sent_messages_m.find(route);
	if (it == sent_messages_m.end()) {
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
		new_messages_m->push_front(mit->second);
	}

	msg_map.clear();
}

bool
message_cache::is_message_expired(cached_message_ptr_t msg) {
	return msg->is_expired();
}

void
message_cache::get_expired_messages(message_queue_t& expired_messages) {
	boost::mutex::scoped_lock lock(mutex_m);

	assert(new_messages_m);

	// remove expired from sent
	route_sent_messages_map_t::iterator it = sent_messages_m.begin();
	for (; it != sent_messages_m.end(); ++it) {

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
	message_queue_t::iterator it2 = new_messages_m->begin();
	while (it2 != new_messages_m->end()) {
		// get single pending message
		boost::shared_ptr<message_iface> msg = *it2;
		assert(msg);

		// remove expired messages
		if (msg->is_expired()) {
			expired_messages.push_back(msg);
		}

		++it2;
	}

	new_messages_m->erase(std::remove_if(new_messages_m->begin(),
										 new_messages_m->end(),
										 &message_cache::is_message_expired),
										 new_messages_m->end());
}

} // namespace dealer
} // namespace cocaine
