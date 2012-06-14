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
#include "cocaine/dealer/core/dealer_object.hpp"
#include "cocaine/dealer/core/message_iface.hpp"

namespace cocaine {
namespace dealer {

class message_cache : private boost::noncopyable, public dealer_object_t {
public:
	typedef boost::shared_ptr<message_iface> cached_message_ptr_t;
	typedef std::deque<cached_message_ptr_t> message_queue_t;
	typedef boost::shared_ptr<message_queue_t> message_queue_ptr_t;
	typedef std::pair<std::string, message_path> message_data_t;
	typedef std::vector<message_data_t> expired_messages_data_t;

	// <uuid, sent message>
	typedef std::map<std::string, cached_message_ptr_t> sent_messages_map_t;

	// <route, sent messages map>
	typedef std::map<std::string, sent_messages_map_t> route_sent_messages_map_t;

public:
	message_cache(const boost::shared_ptr<context_t>& ctx,
				  bool logging_enabled = true);

	virtual ~message_cache();

	void enqueue(const boost::shared_ptr<message_iface>& message);
	void enqueue_with_priority(const boost::shared_ptr<message_iface>& message);
	void append_message_queue(message_queue_ptr_t queue);

	size_t new_messages_count();
	size_t sent_messages_count();

	cached_message_ptr_t get_new_message();
	
	bool get_sent_message(const std::string& route,
						  const std::string& uuid,
						  boost::shared_ptr<message_iface>& message);

	message_queue_ptr_t new_messages();
	void move_new_message_to_sent(const std::string& route);
	void move_sent_message_to_new(const std::string& route, const std::string& uuid);
	void move_sent_message_to_new_front(const std::string& route, const std::string& uuid);
	void remove_message_from_cache(const std::string& route, const std::string& uuid);
	void make_all_messages_new();
	void get_expired_messages(message_queue_t& expired_messages);
	void make_all_messages_new_for_route(const std::string& route);

private:
	static bool is_message_expired(cached_message_ptr_t msg);

private:
	enum e_message_cache_type	type_m;
	route_sent_messages_map_t	sent_messages_m;
	message_queue_ptr_t			new_messages_m;

	boost::mutex mutex_m;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_PERSISTENT_STORAGE_HPP_INCLUDED_
