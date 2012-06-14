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

#ifndef _COCAINE_DEALER_HANDLE_HPP_INCLUDED_
#define _COCAINE_DEALER_HANDLE_HPP_INCLUDED_

#include <string>
#include <map>
#include <memory>
#include <cerrno>

#include <zmq.hpp>

#include <msgpack.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time.hpp>
#include <boost/shared_ptr.hpp>

#include "json/json.h"

#include "cocaine/dealer/structs.hpp"
#include "cocaine/dealer/core/balancer.hpp"
#include "cocaine/dealer/core/handle_info.hpp"
#include "cocaine/dealer/core/message_iface.hpp"
#include "cocaine/dealer/core/message_cache.hpp"
#include "cocaine/dealer/core/dealer_object.hpp"
#include "cocaine/dealer/core/cached_response.hpp"
#include "cocaine/dealer/core/cocaine_endpoint.hpp"
#include "cocaine/dealer/utils/progress_timer.hpp"

namespace cocaine {
namespace dealer {

#define CONTROL_MESSAGE_CONNECT 1
#define CONTROL_MESSAGE_UPDATE 2
#define CONTROL_MESSAGE_DISCONNECT 3
#define CONTROL_MESSAGE_KILL 4

// predeclaration
class handle_t : private boost::noncopyable, public dealer_object_t {
public:
	typedef std::vector<cocaine_endpoint> endpoints_list_t;
	typedef boost::shared_ptr<zmq::socket_t> socket_ptr_t;

	typedef boost::shared_ptr<cached_response_t> cached_response_prt_t;
	typedef boost::function<void(cached_response_prt_t)> responce_callback_t;

public:
	handle_t(const handle_info_t& info,
			 const endpoints_list_t& endpoints,
			 const boost::shared_ptr<context_t>& context,
			 bool logging_enabled = true);

	~handle_t();

	// networking
	void connect();
	void disconnect();
	void update_endpoints(const std::vector<cocaine_endpoint>& endpoints);

	// responses consumer
	void set_responce_callback(responce_callback_t callback);

	// message processing
	void enqueue_message(const boost::shared_ptr<message_iface>& message);
	void make_all_messages_new();
	void assign_message_queue(const message_cache::message_queue_ptr_t& message_queue);

	// info retrieval
	const handle_info_t& info() const;
	std::string description();

	boost::shared_ptr<message_cache> messages_cache() const;

private:
	void kill();
	void dispatch_messages();

	// working with control messages
	void dispatch_control_messages(int type, balancer_t& balancer);
	void establish_control_conection(socket_ptr_t& control_socket);
	int receive_control_messages(socket_ptr_t& control_socket, int poll_timeout);
	bool reshedule_message(const std::string& route, const std::string& uuid);

	// working with messages
	bool dispatch_next_available_message(balancer_t& balancer);
	void dispatch_next_available_response(balancer_t& balancer);
	void process_deadlined_messages();

	// working with responces
	void enqueue_response(cached_response_prt_t response);

	static const int socket_poll_timeout = 100; // millisecs

private:
	handle_info_t info_m;
	endpoints_list_t endpoints_m;
	boost::shared_ptr<message_cache> message_cache_m;

	boost::thread thread_m;
	boost::mutex mutex_;
	volatile bool is_running_m;
	volatile bool is_connected_m;

	std::auto_ptr<zmq::socket_t> zmq_control_socket_m;
	bool receiving_control_socket_ok_m;

	responce_callback_t response_callback_m;

	progress_timer last_response_timer_m;
	progress_timer deadlined_messages_timer_m;
	progress_timer control_messages_timer_m;
};

} // namespace dealer
} // namespace cocaine

#endif // _COCAINE_DEALER_HANDLE_HPP_INCLUDED_
