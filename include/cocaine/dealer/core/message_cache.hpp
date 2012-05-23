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

#ifndef _COCAINE_DEALER_PERSISTENT_STORAGE_HPP_INCLUDED_
#define _COCAINE_DEALER_PERSISTENT_STORAGE_HPP_INCLUDED_

#include <string>
#include <memory>
#include <map>
#include <list>

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

#include <eblob/eblob.hpp>

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/defaults.hpp"
#include "cocaine/dealer/core/context.hpp"
#include "cocaine/dealer/core/message_iface.hpp"

namespace cocaine {
namespace dealer {

class message_cache : private boost::noncopyable {
public:
	typedef boost::shared_ptr<message_iface> cached_message_ptr_t;
	typedef std::deque<cached_message_ptr_t> message_queue_t;
	typedef boost::shared_ptr<message_queue_t> message_queue_ptr_t;
	typedef std::pair<std::string, message_path> message_data_t;
	typedef std::vector<message_data_t> expired_messages_data_t;

	// map <uuid, cached message>
	typedef std::map<std::string, cached_message_ptr_t> messages_index_t;

public:
	explicit message_cache(boost::shared_ptr<cocaine::dealer::context> context,
						   enum e_message_cache_type type);

	virtual ~message_cache();

	void enqueue(const boost::shared_ptr<message_iface>& message);
	void enqueue_with_priority(const boost::shared_ptr<message_iface>& message);
	void append_message_queue(message_queue_ptr_t queue);

	size_t new_messages_count();
	size_t sent_messages_count();
	cached_message_ptr_t get_new_message();
	cached_message_ptr_t get_sent_message(const std::string& uuid);
	message_queue_ptr_t new_messages();
	void move_new_message_to_sent();
	void move_sent_message_to_new(const std::string& uuid);
	void move_sent_message_to_new_front(const std::string& uuid);
	void remove_message_from_cache(const std::string& uuid);
	void make_all_messages_new();
	void get_expired_messages(message_queue_t& expired_messages);

private:
	static bool is_message_expired(cached_message_ptr_t msg);

	boost::shared_ptr<cocaine::dealer::context> context();
	boost::shared_ptr<base_logger> logger();
	boost::shared_ptr<configuration> config();

private:
	boost::shared_ptr<cocaine::dealer::context> context_;
	enum e_message_cache_type type_;

	messages_index_t sent_messages_;
	message_queue_ptr_t new_messages_;

	boost::mutex mutex_;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_PERSISTENT_STORAGE_HPP_INCLUDED_
