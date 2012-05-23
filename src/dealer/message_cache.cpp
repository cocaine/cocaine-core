//
// Copyright (C) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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

message_cache::message_cache(boost::shared_ptr<cocaine::dealer::context> context,
							 enum e_message_cache_type type) :
	context_(context),
	type_(type)
{
	new_messages_.reset(new message_queue_t);
}

message_cache::~message_cache() {
}

boost::shared_ptr<cocaine::dealer::context>
message_cache::context() {
	return context_;
}

boost::shared_ptr<base_logger>
message_cache::logger() {
	return context()->logger();
}

boost::shared_ptr<configuration>
message_cache::config() {
	boost::shared_ptr<configuration> conf = context()->config();
	if (!conf.get()) {
		throw internal_error("configuration object is empty at: " + std::string(BOOST_CURRENT_FUNCTION));
	}

	return conf;
}

message_cache::message_queue_ptr_t
message_cache::new_messages() {
	if (!new_messages_) {
		std::string error_str = "new messages queue object is empty at ";
		error_str += std::string(BOOST_CURRENT_FUNCTION);
		throw internal_error(error_str);
	}

	return new_messages_;
}

void
message_cache::enqueue_with_priority(const boost::shared_ptr<message_iface>& message) {
	boost::mutex::scoped_lock lock(mutex_);
	new_messages_->push_front(message);
}

void
message_cache::enqueue(const boost::shared_ptr<message_iface>& message) {
	boost::mutex::scoped_lock lock(mutex_);
	new_messages_->push_back(message);
}

void
message_cache::append_message_queue(message_queue_ptr_t queue) {
	boost::mutex::scoped_lock lock(mutex_);

	// validate new queue
	if (!queue || queue->empty()) {
		return;
	}

	// append messages
	new_messages_->insert(new_messages_->end(), queue->begin(), queue->end());
}

boost::shared_ptr<message_iface>
message_cache::get_new_message() {
	boost::mutex::scoped_lock lock(mutex_);
	return new_messages_->front();
}

size_t
message_cache::new_messages_count() {
	boost::mutex::scoped_lock lock(mutex_);
	return new_messages_->size();
}

size_t
message_cache::sent_messages_count() {
	boost::mutex::scoped_lock lock(mutex_);

	size_t sent_messages_count = 0;

	endpoints_sent_messages_map_t::const_iterator it = sent_messages_.begin();
	for (; it != sent_messages_.end(); ++it) {
		sent_messages_count += it->second.size();
	}

	return sent_messages_count;
}

boost::shared_ptr<message_iface>
message_cache::get_sent_message(const std::string& endpoint, const std::string& uuid) {
	boost::mutex::scoped_lock lock(mutex_);
	endpoints_sent_messages_map_t::const_iterator it = sent_messages_.find(endpoint);

	if (it == sent_messages_.end()) {
		std::string error_str = "can't find sent messages for endpoint " + endpoint;
		throw internal_error(error_str);
	}

	const sent_messages_map_t& msg_map = it->second;
	sent_messages_map_t::const_iterator mit = msg_map.find(uuid);


	if (!mit->second) {
		throw internal_error("empty cached message object at " + std::string(BOOST_CURRENT_FUNCTION));
	}

	return mit->second;
}

void
message_cache::move_new_message_to_sent(const std::string& endpoint) {
	boost::mutex::scoped_lock lock(mutex_);

	boost::shared_ptr<message_iface> msg = new_messages_->front();
	assert(msg);

	endpoints_sent_messages_map_t::iterator it = sent_messages_.find(endpoint);
	if (it == sent_messages_.end()) {
		sent_messages_map_t msg_map;
		msg_map.insert(std::make_pair(msg->uuid(), msg));
		sent_messages_[endpoint] = msg_map;
	}
	else {
		it->second.insert(std::make_pair(msg->uuid(), msg));
	}

	new_messages_->pop_front();
}

void
message_cache::move_sent_message_to_new(const std::string& endpoint, const std::string& uuid) {
	boost::mutex::scoped_lock lock(mutex_);
	endpoints_sent_messages_map_t::iterator it = sent_messages_.find(endpoint);

	if (it == sent_messages_.end()) {
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

	new_messages_->push_back(msg);
}

void
message_cache::move_sent_message_to_new_front(const std::string& endpoint, const std::string& uuid) {
	boost::mutex::scoped_lock lock(mutex_);
	endpoints_sent_messages_map_t::iterator it = sent_messages_.find(endpoint);

	if (it == sent_messages_.end()) {
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

	new_messages_->push_front(msg);
}

void
message_cache::remove_message_from_cache(const std::string& endpoint, const std::string& uuid) {
	boost::mutex::scoped_lock lock(mutex_);
	endpoints_sent_messages_map_t::iterator it = sent_messages_.find(endpoint);

	if (it == sent_messages_.end()) {
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
}

void
message_cache::make_all_messages_new() {
	boost::mutex::scoped_lock lock(mutex_);
	endpoints_sent_messages_map_t::iterator it = sent_messages_.begin();
	for (; it != sent_messages_.end(); ++it) {

		sent_messages_map_t& msg_map = it->second;
		sent_messages_map_t::iterator mit = msg_map.begin();

		for (; mit != msg_map.end(); ++mit) {

			if (!mit->second) {
				throw internal_error("empty cached message object at " + std::string(BOOST_CURRENT_FUNCTION));
			}

			mit->second->mark_as_sent(false);
			mit->second->set_ack_received(false);
			new_messages_->push_front(mit->second);
		}

		msg_map.clear();
	}
}

bool
message_cache::is_message_expired(cached_message_ptr_t msg) {
	return msg->is_expired();
}

void
message_cache::get_expired_messages(message_queue_t& expired_messages) {
	boost::mutex::scoped_lock lock(mutex_);

	assert(new_messages_);

	// remove expired from sent
	endpoints_sent_messages_map_t::iterator it = sent_messages_.begin();
	for (; it != sent_messages_.end(); ++it) {

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
	message_queue_t::iterator it2 = new_messages_->begin();
	while (it2 != new_messages_->end()) {
		// get single pending message
		boost::shared_ptr<message_iface> msg = *it2;
		assert(msg);

		// remove expired messages
		if (msg->is_expired()) {
			expired_messages.push_back(msg);
		}

		++it2;
	}

	new_messages_->erase(std::remove_if(new_messages_->begin(),
										new_messages_->end(),
										&message_cache::is_message_expired),
						 new_messages_->end());
}

} // namespace dealer
} // namespace cocaine
